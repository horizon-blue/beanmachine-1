# Copyright (c) Facebook, Inc. and its affiliates.

from typing import Optional

import beanmachine.ppl.compiler.bmg_nodes as bn
from beanmachine.ppl.compiler.bm_graph_builder import BMGraphBuilder
from beanmachine.ppl.compiler.fix_problem import ProblemFixerBase
from beanmachine.ppl.compiler.lattice_typer import LatticeTyper


class LogSumExpFixer(ProblemFixerBase):
    """This class takes a Bean Machine Graph builder and attempts to
    rewrite log expressions of the form:

    * log( exp(a) + exp(b) + exp(c) ...) -> logsumexp(a,b,c, ...)

    Note that this transformation depends on MultiaryAdditionFixer.
    """

    def __init__(self, bmg: BMGraphBuilder, typer: LatticeTyper) -> None:
        ProblemFixerBase.__init__(self, bmg, typer)

    def _is_exp_input(self, n: bn.MultiAdditionNode) -> bool:
        return all(isinstance(i, bn.ExpNode) for i in n.inputs)

    def can_be_log_sum_exp(self, n: bn.LogNode) -> bool:
        o = n.operand
        return isinstance(o, bn.MultiAdditionNode) and self._is_exp_input(o)

    def _needs_fixing(self, n: bn.BMGNode) -> bool:
        # A log expression is fixable if operand is a MultiAdditionNode.
        # Additionally, each input of this MultiAdditionNode is an ExpNode

        return isinstance(n, bn.LogNode) and self.can_be_log_sum_exp(n)

    def _get_replacement(self, n: bn.BMGNode) -> Optional[bn.BMGNode]:
        assert isinstance(n, bn.LogNode)
        assert self.can_be_log_sum_exp(n)

        acc = []
        for c in n.operand.inputs:
            assert isinstance(c, bn.ExpNode)
            acc.append(c.operand)

        assert len(acc) == len(n.operand.inputs)
        return self._bmg.add_logsumexp(*acc)