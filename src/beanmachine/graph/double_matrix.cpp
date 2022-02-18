/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "beanmachine/graph/double_matrix.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <thread>
#include <variant>

namespace beanmachine {
namespace graph {

// Conveniences for this compilation unit

using std::get;
using Matrix = DoubleMatrix::Matrix;

template <typename T>
bool has(const DoubleMatrix& double_matrix) {
  return std::holds_alternative<T>(double_matrix);
}

/// MatrixProperty methods

MatrixProperty::MatrixProperty(DoubleMatrix& owner) : owner(&owner) {}

inline Matrix& MatrixProperty::value() {
  return get<Matrix>(*owner);
}

inline const Matrix& MatrixProperty::value() const {
  return get<Matrix>(*owner);
}

Matrix& MatrixProperty::operator=(const Matrix& m) {
  owner->VariantBaseClass::operator=(m);
  return value();
}

MatrixProperty::operator const Matrix&() const {
  return value();
}

double MatrixProperty::coeff(Eigen::MatrixXd::Index i) const {
  return std::get<Eigen::MatrixXd>(*owner).coeff(i);
}

double& MatrixProperty::operator()(Eigen::MatrixXd::Index i) {
  return std::get<Eigen::MatrixXd>(*owner)(i);
}

double& MatrixProperty::operator()(
    Eigen::MatrixXd::Index row,
    Eigen::MatrixXd::Index col) {
  return std::get<Eigen::MatrixXd>(*owner)(row, col);
}

Eigen::MatrixXd::ColXpr MatrixProperty::col(Eigen::MatrixXd::Index i) {
  return std::get<Eigen::MatrixXd>(*owner).col(i);
}

double MatrixProperty::sum() {
  return value().sum();
}

Matrix& MatrixProperty::operator+=(const Matrix& increment) {
  return value() += increment;
}

Matrix& MatrixProperty::operator+=(const DoubleMatrix& increment) {
  return value() += get<Matrix>(increment);
}

Matrix& MatrixProperty::operator-=(const Matrix& increment) {
  return value() -= increment;
}

Matrix MatrixProperty::operator*(const Matrix& increment) {
  return value() * increment;
}

Matrix MatrixProperty::operator*(const DoubleMatrix& increment) {
  return value() * get<Matrix>(increment);
}

Matrix operator*(const Matrix& operand, const MatrixProperty& mp) {
  return operand * get<Matrix>(*mp.owner);
}

Matrix operator*(double operand, const MatrixProperty& mp) {
  return operand * get<Matrix>(*mp.owner);
}

Matrix::ColXpr operator+=(Matrix::ColXpr operand, const MatrixProperty& mp) {
  return operand += get<Matrix>(*mp.owner);
}

Eigen::MatrixXd& MatrixProperty::setZero(
    Eigen::MatrixXd::Index rows,
    Eigen::MatrixXd::Index cols) {
  if (not has<Matrix>(*owner)) {
    *this = Eigen::MatrixXd();
  }
  return value().setZero(rows, cols);
}

Eigen::ArrayWrapper<Matrix> MatrixProperty::array() {
  return value().array();
}

Matrix::Scalar* MatrixProperty::data() {
  return value().data();
}

Matrix::Index MatrixProperty::size() {
  return value().size();
}

/// DoubleMatrix methods

#define TYPE(DM) ((DM).index())
#define DOUBLE 0
#define MATRIX 1

DoubleMatrixError double_matrix_error(const char* message) {
  return DoubleMatrixError(message);
}

/// Conversions

DoubleMatrix::operator double() const {
  if (not has<double>(*this)) {
    throw double_matrix_error(
        "operator double() on DoubleMatrix without double");
  }
  return get<double>(*this);
}

/// =

DoubleMatrix& DoubleMatrix::operator=(double d) {
  VariantBaseClass::operator=(d);
  return *this;
}

DoubleMatrix& DoubleMatrix::operator=(const Matrix& matrix) {
  VariantBaseClass::operator=(matrix);
  return *this;
}

DoubleMatrix& DoubleMatrix::operator=(const DoubleMatrix& double_matrix) {
  switch (TYPE(double_matrix)) {
    case DOUBLE:
      VariantBaseClass::operator=(get<double>(double_matrix));
      return *this;
    case MATRIX:
      VariantBaseClass::operator=(get<Matrix>(double_matrix));
      return *this;
    default:
      throw double_matrix_error("Assigning from DoubleMatrix without value");
  }
}

/// array

DoubleMatrix::Array DoubleMatrix::array() {
  return get<Matrix>(*this).array();
}

const DoubleMatrix::ArrayOfConst DoubleMatrix::array() const {
  return get<Matrix>(*this).array();
}

/// +=

DoubleMatrix& DoubleMatrix::operator+=(double d) {
  switch (TYPE(*this)) {
    case DOUBLE:
      get<double>(*this) += d;
      return *this;
    case MATRIX:
      throw double_matrix_error(
          "In-place addition of double to 'DoubleMatrix' containing matrix");
    default:
      throw double_matrix_error("In-place addition to empty DoubleMatrix");
  }
}

DoubleMatrix& DoubleMatrix::operator+=(const Matrix& matrix) {
  switch (TYPE(*this)) {
    case DOUBLE:
      throw double_matrix_error(
          "In-place addition of matrix to 'DoubleMatrix' containing double");
    case MATRIX:
      get<Matrix>(*this) += matrix;
      return *this;
    default:
      throw double_matrix_error("In-place addition to empty DoubleMatrix");
  }
}

DoubleMatrix& DoubleMatrix::operator+=(const DoubleMatrix& another) {
  switch (TYPE(*this)) {
    case DOUBLE:
      get<double>(*this) += get<double>(another);
      return *this;
    case MATRIX:
      get<Matrix>(*this) += get<Matrix>(another);
      return *this;
    default:
      throw double_matrix_error("In-place addition to empty DoubleMatrix");
  }
}

/// -=

DoubleMatrix& DoubleMatrix::operator-=(double d) {
  switch (TYPE(*this)) {
    case DOUBLE:
      get<double>(*this) -= d;
      return *this;
    case MATRIX:
      throw double_matrix_error(
          "In-place subtraction of double to 'DoubleMatrix' containing matrix");
    default:
      throw double_matrix_error("In-place subtraction to empty DoubleMatrix");
  }
}

DoubleMatrix& DoubleMatrix::operator-=(const Matrix& matrix) {
  switch (TYPE(*this)) {
    case DOUBLE:
      throw double_matrix_error(
          "In-place subtraction of matrix to 'DoubleMatrix' containing double");
    case MATRIX:
      get<Matrix>(*this) -= matrix;
      return *this;
    default:
      throw double_matrix_error("In-place subtraction to empty DoubleMatrix");
  }
}

DoubleMatrix& DoubleMatrix::operator-=(const DoubleMatrix& another) {
  switch (TYPE(*this)) {
    case DOUBLE:
      get<double>(*this) -= get<double>(another);
      return *this;
    case MATRIX:
      get<Matrix>(*this) -= get<Matrix>(another);
      return *this;
    default:
      throw double_matrix_error("In-place subtraction to empty DoubleMatrix");
  }
}

/// operator()

double& DoubleMatrix::operator()(int i) {
  return get<Matrix>(*this)(i);
}

double& DoubleMatrix::operator()(int row, int col) {
  return get<Matrix>(*this)(row, col);
}

double DoubleMatrix::operator()(int i) const {
  return get<Matrix>(*this)(i);
}

double DoubleMatrix::operator()(int row, int col) const {
  return get<Matrix>(*this)(row, col);
}

/// setZero

DoubleMatrix& DoubleMatrix::setZero(int rows, int cols) {
  if (not has<Matrix>(*this)) {
    *this = Matrix();
  }
  get<Matrix>(*this).setZero(rows, cols);
  return *this;
}

/// *

DoubleMatrix operator*(const DoubleMatrix& double_matrix, double d) {
  switch (TYPE(double_matrix)) {
    case DOUBLE:
      return DoubleMatrix{get<double>(double_matrix) * d};
    case MATRIX:
      return DoubleMatrix{get<Matrix>(double_matrix) * d};
    default:
      throw double_matrix_error(
          "Multiplying DoubleMatrix that does not hold a value.");
  }
}

DoubleMatrix operator*(double d, const DoubleMatrix& double_matrix) {
  return double_matrix * d;
}

DoubleMatrix operator*(const DoubleMatrix& dm1, const DoubleMatrix& dm2) {
  switch (TYPE(dm1)) {
    case DOUBLE:
      return get<double>(dm1) * dm2;
    case MATRIX:
      if (has<Matrix>(dm2)) {
        return DoubleMatrix{get<Matrix>(dm1) * get<Matrix>(dm2)};
      } else if (has<double>(dm2)) {
        return DoubleMatrix{get<Matrix>(dm1) * get<double>(dm2)};
      } else {
        throw double_matrix_error(
            "Multiplying DoubleMatrix that does not hold a value.");
      }
    default:
      throw double_matrix_error(
          "Multiplying DoubleMatrix that does not hold a value.");
  }
}

/// +

// Note: Eigen does not support adding a matrix and a double, so
// here we can assume arguments will always contain information
// of the same type.

double operator+(const DoubleMatrix& double_matrix, double d) {
  switch (TYPE(double_matrix)) {
    case DOUBLE:
      return get<double>(double_matrix) + d;
    case MATRIX:
      throw double_matrix_error(
          "Adding DoubleMatrix with matrix to double is not supported by Eigen.");
    default:
      throw double_matrix_error(
          "Adding DoubleMatrix that does not hold a value.");
  }
}

double operator+(double d, const DoubleMatrix& double_matrix) {
  return double_matrix + d;
}

Matrix operator+(const DoubleMatrix& double_matrix, const Matrix& matrix) {
  switch (TYPE(double_matrix)) {
    case DOUBLE:
      throw double_matrix_error(
          "Adding DoubleMatrix with double to a matrix is not supported by Eigen.");
    case MATRIX:
      return get<Matrix>(double_matrix) + matrix;
    default:
      throw double_matrix_error(
          "Adding DoubleMatrix that does not hold a value.");
  }
}

Matrix operator+(const Matrix& matrix, const DoubleMatrix& double_matrix) {
  switch (TYPE(double_matrix)) {
    case DOUBLE:
      throw double_matrix_error(
          "Adding DoubleMatrix with double to a matrix is not supported by Eigen.");
    case MATRIX:
      return matrix + get<Matrix>(double_matrix);
    default:
      throw double_matrix_error(
          "Adding DoubleMatrix that does not hold a value.");
  }
}

DoubleMatrix operator+(const DoubleMatrix& dm1, const DoubleMatrix& dm2) {
  switch (TYPE(dm1)) {
    case DOUBLE:
      return DoubleMatrix{get<double>(dm1) + dm2};
    case MATRIX:
      return DoubleMatrix{get<Matrix>(dm1) + get<Matrix>(dm2)};
    default:
      throw double_matrix_error(
          "Adding DoubleMatrix that does not hold a value.");
  }
}

#undef MATRIX
#undef DOUBLE
#undef TYPE

} // namespace graph
} // namespace beanmachine