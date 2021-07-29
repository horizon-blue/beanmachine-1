// Copyright (c) Facebook, Inc. and its affiliates.

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "beanmachine/graph/distribution/distribution.h"
#include "beanmachine/graph/graph.h"
#include "beanmachine/graph/nmc.h"
#include "beanmachine/graph/operator/stochasticop.h"
#include "beanmachine/graph/profiler.h"
#include "beanmachine/graph/proposer/default_initializer.h"
#include "beanmachine/graph/proposer/proposer.h"
#include "beanmachine/graph/util.h"

#include "beanmachine/graph/stepper/nmc_dirichlet_gamma_single_site_stepper.h"

namespace beanmachine {
namespace graph {

bool NMCDirichletGammaSingleSiteStepper::is_applicable_to(
    graph::Node* tgt_node) {
  return tgt_node->value.type.variable_type == VariableType::COL_SIMPLEX_MATRIX;
}

/*
We treat the K-dimensional Dirichlet sample as K independent Gamma samples
divided by their sum. i.e. Let X_k ~ Gamma(alpha_k, 1), for k = 1, ..., K,
Y_k = X_k / sum(X), then (Y_1, ..., Y_K) ~ Dirichlet(alphas). We store Y in
the attribute value, and X in unconstrainted_value.
*/
void NMCDirichletGammaSingleSiteStepper::step(
    Node* tgt_node,
    const std::vector<Node*>& det_nodes,
    const std::vector<Node*>& sto_nodes) {
  graph->pd_begin(ProfilerEvent::NMC_STEP_DIRICHLET);
  uint K = tgt_node->value._matrix.size();
  // Cast needed to access fields such as unconstrained_value:
  auto src_node = static_cast<oper::StochasticOperator*>(tgt_node);
  // @lint-ignore CLANGTIDY
  auto dirichlet_distribution_node = src_node->in_nodes[0];
  auto param_node = dirichlet_distribution_node->in_nodes[0];
  for (uint k = 0; k < K; k++) {
    // Prepare gradients
    // Grad1 = (dY_1/dX_k, dY_2/dX_k, ..., dY_K/X_k)
    // where dY_k/dX_k = (sum(X) - X_k)/sum(X)^2
    //       dY_j/dX_k = - X_j/sum(X)^2, for j != k
    // Grad2 = (d^2Y_1/dX^2_k, ..., d^2Y_K/X^2_k)
    // where d2Y_k/dX2_k = -2 * (sum(X) - X_k)/sum(X)^3
    //       d2Y_j/dX2_k = -2 * X_j/sum(X)^3
    double param_a = param_node->value._matrix.coeff(k);
    double old_X_k = src_node->unconstrained_value._matrix.coeff(k);
    double sum = src_node->unconstrained_value._matrix.sum();
    src_node->Grad1 =
        -src_node->unconstrained_value._matrix.array() / (sum * sum);
    *(src_node->Grad1.data() + k) += 1 / sum;
    src_node->Grad2 = src_node->Grad1 * (-2.0) / sum;
    src_node->grad1 = 1;
    src_node->grad2 = 0;
    // Propagate gradients
    NodeValue old_value(AtomicType::POS_REAL, old_X_k);
    nmc->save_old_values(det_nodes);
    nmc->compute_gradients(det_nodes);
    double old_sto_affected_nodes_log_prob;
    auto old_prop = create_proposer_dirichlet_gamma(
        sto_nodes,
        tgt_node,
        param_a,
        old_value,
        /* out */ old_sto_affected_nodes_log_prob);

    NodeValue new_value = nmc->sample(old_prop);

    *(src_node->unconstrained_value._matrix.data() + k) = new_value._double;
    sum = src_node->unconstrained_value._matrix.sum();
    src_node->value._matrix =
        src_node->unconstrained_value._matrix.array() / sum;

    // Propagate values and gradients at new value of X_k
    src_node->Grad1 =
        -src_node->unconstrained_value._matrix.array() / (sum * sum);
    *(src_node->Grad1.data() + k) += 1 / sum;
    src_node->Grad2 = src_node->Grad1 * (-2.0) / sum;
    nmc->eval(det_nodes);
    nmc->compute_gradients(det_nodes);

    double new_sto_affected_nodes_log_prob;
    auto new_prop = create_proposer_dirichlet_gamma(
        sto_nodes,
        tgt_node,
        param_a,
        new_value,
        /* out */ new_sto_affected_nodes_log_prob);
    double logacc = new_sto_affected_nodes_log_prob -
        old_sto_affected_nodes_log_prob + new_prop->log_prob(old_value) -
        old_prop->log_prob(new_value);
    // Accept or reject, reset (values and) gradients
    bool accepted = logacc > 0 or util::sample_logprob(nmc->gen, logacc);
    if (!accepted) {
      nmc->restore_old_values(det_nodes);
      *(src_node->unconstrained_value._matrix.data() + k) = old_X_k;
      sum = src_node->unconstrained_value._matrix.sum();
      src_node->value._matrix =
          src_node->unconstrained_value._matrix.array() / sum;
    }
    // Gradients are must be cleared (equal to 0)
    // at the end of each iteration.
    // Some code relies on that to decide whether a node
    // is the one we are computing gradients with respect to.
    nmc->clear_gradients(det_nodes);
    tgt_node->grad1 = 0;
    tgt_node->grad2 = 0;
  } // k
  graph->pd_finish(ProfilerEvent::NMC_STEP_DIRICHLET);
}

std::unique_ptr<proposer::Proposer>
NMCDirichletGammaSingleSiteStepper::create_proposer_dirichlet_gamma(
    const std::vector<Node*>& sto_nodes,
    Node* tgt_node,
    double param_a,
    NodeValue value,
    /* out */ double& logweight) {
  // TODO: Reorganize in the same manner the default NMC
  // proposer has been reorganized
  graph->pd_begin(ProfilerEvent::NMC_CREATE_PROP_DIR);
  logweight = 0;
  double grad1 = 0;
  double grad2 = 0;
  for (Node* node : sto_nodes) {
    if (node == tgt_node) {
      // TODO: unify this computation of logweight
      // and grad1, grad2 with those present in methods
      // log_prob and gradient_log_prob

      // X_k ~ Gamma(param_a, 1)
      // PDF of Gamma(a, 1) is x^(a - 1)exp(-x)/gamma(a)
      // so log pdf(x) = log(x^(a - 1)) + (-x) - log(gamma(a))
      // = (a - 1)*log(x) - x - log(gamma(a))
      logweight += (param_a - 1.0) * std::log(value._double) - value._double -
          lgamma(param_a);
      grad1 += (param_a - 1.0) / value._double - 1.0;
      grad2 += (1.0 - param_a) / (value._double * value._double);
    } else {
      logweight += node->log_prob();
      node->gradient_log_prob(tgt_node, /* in-out */ grad1, /* in-out */ grad2);
    }
  }
  std::unique_ptr<proposer::Proposer> prop =
      proposer::nmc_proposer(value, grad1, grad2);
  graph->pd_finish(ProfilerEvent::NMC_CREATE_PROP_DIR);
  return prop;
}

} // namespace graph
} // namespace beanmachine