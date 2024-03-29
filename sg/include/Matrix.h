//
// SG
// Matrix.h
//
// Copyright © 2020-2021 Gustavo C. Viegas.
//

#ifndef YF_SG_MATRIX_H
#define YF_SG_MATRIX_H

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <initializer_list>
#include <type_traits>

#include "yf/sg/Defs.h"
#include "yf/sg/Vector.h"
#include "yf/sg/Quaternion.h"

SG_NS_BEGIN

/// Matrix.
///
template<class T, size_t colN, size_t rowN>
class Matrix {
  static_assert(colN > 0 && rowN > 0, "Zero-sized Matrix not supported");
  static_assert(std::is_arithmetic<T>(), "Matrix must be a numeric type");

 public:
  Matrix() = default;
  Matrix(const Matrix&) = default;
  Matrix& operator=(const Matrix&) = default;
  ~Matrix() = default;

  /// Single value construction that sets the matrix's diagonal.
  ///
  constexpr Matrix(T scalar) {
    if (colN <= rowN) {
      for (size_t i = 0; i < colN; ++i)
        data_[i][i] = scalar;
    } else {
      for (size_t i = 0; i < rowN; ++i)
        data_[i][i] = scalar;
    }
  }

  /// Initializer-list construction from column vectors.
  ///
  /// Missing components are default-initialized.
  ///
  constexpr Matrix(std::initializer_list<Vector<T, rowN>> list) {
    for (size_t i = 0; i < std::min(colN, list.size()); ++i)
      data_[i] = *(list.begin()+i);
  }

  /// Identity matrix.
  ///
  static constexpr Matrix identity() {
    static_assert(colN == rowN, "identity() requires a square matrix");

    Matrix mat;
    for (size_t i = 0; i < colN; ++i)
      mat[i][i] = static_cast<T>(1);
    return mat;
  }

  constexpr const Vector<T, rowN>& operator[](size_t column) const {
    return data_[column];
  }

  constexpr Vector<T, rowN>& operator[](size_t column) {
    return data_[column];
  }

  constexpr const Vector<T, rowN>* begin() const {
    return data_;
  }

  constexpr Vector<T, rowN>* begin() {
    return data_;
  }

  constexpr const Vector<T, rowN>* end() const {
    return data_+colN;
  }

  constexpr Vector<T, rowN>* end() {
    return data_+colN;
  }

  constexpr const T* data() const {
    return data_[0].data();
  }

  constexpr T* data() {
    return data_[0].data();
  }

  /// Size of matrix data, in bytes.
  ///
  static constexpr size_t dataSize() {
    return sizeof data_;
  }

  /// Number of columns in the matrix.
  ///
  static constexpr size_t columns() {
    return colN;
  }

  /// Number of rows in the matrix.
  ///
  static constexpr size_t rows() {
    return rowN;
  }

  /// In-place subtraction.
  ///
  constexpr Matrix& operator-=(const Matrix& other) {
    for (size_t i = 0; i < colN; ++i)
      data_[i] -= other[i];
    return *this;
  }

  /// In-place addition.
  ///
  constexpr Matrix& operator+=(const Matrix& other) {
    for (size_t i = 0; i < colN; ++i)
      data_[i] += other[i];
    return *this;
  }

  /// In-place multiplication.
  ///
  constexpr Matrix& operator*=(const Matrix& other) {
    static_assert(colN == rowN,
                  "operator*=() not implemented for non-square matrices");

    const auto tmp = *this;
    for (size_t i = 0; i < colN; ++i) {
      for (size_t j = 0; j < colN; ++j) {
        data_[i][j] = 0;
        for (size_t k = 0; k < colN; ++k)
          data_[i][j] += tmp[k][j] * other[i][k];
      }
    }
    return *this;
  }

  /// In-place transpose operation.
  ///
  constexpr Matrix& transpose() {
    static_assert(colN == rowN,
                  "transpose() not implemented for non-square matrices");

    for (size_t i = 0; i < colN; ++i) {
      for (size_t j = i+1; j < colN; ++j)
        std::swap(data_[i][j], data_[j][i]);
    }
    return *this;
  }

 private:
  Vector<T, rowN> data_[colN]{};
};

/// Matrix subtraction.
///
template<class T, size_t colN, size_t rowN>
constexpr Matrix<T, colN, rowN> operator-(const Matrix<T, colN, rowN>& left,
                                          const Matrix<T, colN, rowN>& right) {
  Matrix<T, colN, rowN> res;
  for (size_t i = 0; i < colN; ++i)
    res[i] = left[i] - right[i];
  return res;
}

/// Matrix addition.
///
template<class T, size_t colN, size_t rowN>
constexpr Matrix<T, colN, rowN> operator+(const Matrix<T, colN, rowN>& left,
                                          const Matrix<T, colN, rowN>& right) {
  Matrix<T, colN, rowN> res;
  for (size_t i = 0; i <colN; ++i)
    res[i] = left[i] + right[i];
  return res;
}

/// Matrix multiplication.
///
template<class T, size_t colN, size_t rowN>
constexpr Matrix<T, colN, rowN> operator*(const Matrix<T, colN, rowN>& m1,
                                          const Matrix<T, colN, rowN>& m2) {
  static_assert(colN == rowN,
                "operator*() not implemented for non-square matrices");

  Matrix<T, colN, rowN> res;
  for (size_t i = 0; i < colN; ++i) {
    for (size_t j = 0; j < rowN; ++j) {
      for (size_t k = 0; k < colN; ++k)
        res[i][j] += m1[k][j] * m2[i][k];
    }
  }
  return res;
}

/// Matrix-Vector multiplication.
///
template<class T, size_t colN, size_t rowN>
constexpr Vector<T, rowN> operator*(const Matrix<T, colN, rowN>& mat,
                                    const Vector<T, rowN>& vec) {
  static_assert(colN == rowN,
                "operator*() not implemented for non-square matrices");

  Vector<T, rowN> res;
  for (size_t i = 0; i < rowN; ++i) {
    for (size_t j = 0; j < colN; ++j)
      res[i] += mat[j][i] * vec[j];
  }
  return res;
}

/// Matrix-Vector multiplication with assignment.
///
template<class T, size_t colN, size_t rowN>
constexpr Vector<T, rowN>& operator*=(Vector<T, rowN>& vec,
                                      const Matrix<T, colN, rowN>& mat) {
  static_assert(colN == rowN,
                "operator*=() not implemented for non-square matrices");

  vec = mat * vec;
  return vec;
}

/// Matrix transpose operation.
///
template<class T, size_t colN, size_t rowN>
constexpr Matrix<T, colN, rowN> transpose(const Matrix<T, colN, rowN>& mat) {
  static_assert(colN == rowN,
                "transpose() not implemented for non-square matrices");

  Matrix<T, colN, rowN> res;
  for (size_t i = 0; i < colN; ++i) {
    res[i][i] = mat[i][i];
    for (size_t j = i+1; j < colN; ++j) {
      res[i][j] = mat[j][i];
      res[j][i] = mat[i][j];
    }
  }
  return res;
}

/// Matrix inversion (2x2).
///
template<class T>
constexpr Matrix<T, 2, 2> invert(const Matrix<T, 2, 2>& mat) {
  static_assert(std::is_floating_point<T>(),
                "invert() requires a floating point type");

  const T idet = 1.0 / (mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0]);
  return {{mat[1][1]*idet, mat[0][1]*idet}, {-mat[1][0]*idet, mat[0][0]*idet}};
}

/// Matrix inversion (3x3).
///
template<class T>
constexpr Matrix<T, 3, 3> invert(const Matrix<T, 3, 3>& mat) {
  static_assert(std::is_floating_point<T>(),
                "invert() requires a floating point type");

  Matrix<T, 3, 3> res;

  const T s0 = mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1];
  const T s1 = mat[1][0] * mat[2][2] - mat[1][2] * mat[2][0];
  const T s2 = mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0];
  const T idet = 1.0 / (mat[0][0]*s0 - mat[0][1]*s1 + mat[0][2]*s2);

  res[0][0] = +s0 * idet;
  res[0][1] = -(mat[0][1] * mat[2][2] - mat[0][2] * mat[2][1]) * idet;
  res[0][2] = +(mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1]) * idet;
  res[1][0] = -s1 * idet;
  res[1][1] = +(mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0]) * idet;
  res[1][2] = -(mat[0][0] * mat[1][2] - mat[0][2] * mat[1][0]) * idet;
  res[2][0] = +s2 * idet;
  res[2][1] = -(mat[0][0] * mat[2][1] - mat[0][1] * mat[2][0]) * idet;
  res[2][2] = +(mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0]) * idet;

  return res;
}

/// Matrix inversion (4x4).
///
template<class T>
constexpr Matrix<T, 4, 4> invert(const Matrix<T, 4, 4>& mat) {
  static_assert(std::is_floating_point<T>(),
                "invert() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T s0 = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
  const T s1 = mat[0][0] * mat[1][2] - mat[0][2] * mat[1][0];
  const T s2 = mat[0][0] * mat[1][3] - mat[0][3] * mat[1][0];
  const T s3 = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];
  const T s4 = mat[0][1] * mat[1][3] - mat[0][3] * mat[1][1];
  const T s5 = mat[0][2] * mat[1][3] - mat[0][3] * mat[1][2];
  const T c0 = mat[2][0] * mat[3][1] - mat[2][1] * mat[3][0];
  const T c1 = mat[2][0] * mat[3][2] - mat[2][2] * mat[3][0];
  const T c2 = mat[2][0] * mat[3][3] - mat[2][3] * mat[3][0];
  const T c3 = mat[2][1] * mat[3][2] - mat[2][2] * mat[3][1];
  const T c4 = mat[2][1] * mat[3][3] - mat[2][3] * mat[3][1];
  const T c5 = mat[2][2] * mat[3][3] - mat[2][3] * mat[3][2];
  const T idet = 1.0 / (s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0);

  res[0][0] = (+c5 * mat[1][1] - c4 * mat[1][2] + c3 * mat[1][3]) * idet;
  res[0][1] = (-c5 * mat[0][1] + c4 * mat[0][2] - c3 * mat[0][3]) * idet;
  res[0][2] = (+s5 * mat[3][1] - s4 * mat[3][2] + s3 * mat[3][3]) * idet;
  res[0][3] = (-s5 * mat[2][1] + s4 * mat[2][2] - s3 * mat[2][3]) * idet;
  res[1][0] = (-c5 * mat[1][0] + c2 * mat[1][2] - c1 * mat[1][3]) * idet;
  res[1][1] = (+c5 * mat[0][0] - c2 * mat[0][2] + c1 * mat[0][3]) * idet;
  res[1][2] = (-s5 * mat[3][0] + s2 * mat[3][2] - s1 * mat[3][3]) * idet;
  res[1][3] = (+s5 * mat[2][0] - s2 * mat[2][2] + s1 * mat[2][3]) * idet;
  res[2][0] = (+c4 * mat[1][0] - c2 * mat[1][1] + c0 * mat[1][3]) * idet;
  res[2][1] = (-c4 * mat[0][0] + c2 * mat[0][1] - c0 * mat[0][3]) * idet;
  res[2][2] = (+s4 * mat[3][0] - s2 * mat[3][1] + s0 * mat[3][3]) * idet;
  res[2][3] = (-s4 * mat[2][0] + s2 * mat[2][1] - s0 * mat[2][3]) * idet;
  res[3][0] = (-c3 * mat[1][0] + c1 * mat[1][1] - c0 * mat[1][2]) * idet;
  res[3][1] = (+c3 * mat[0][0] - c1 * mat[0][1] + c0 * mat[0][2]) * idet;
  res[3][2] = (-s3 * mat[3][0] + s1 * mat[3][1] - s0 * mat[3][2]) * idet;
  res[3][3] = (+s3 * mat[2][0] - s1 * mat[2][1] + s0 * mat[2][2]) * idet;

  return res;
}

/// Matrix rotation (3x3).
///
template<class T>
constexpr Matrix<T, 3, 3> rotate3(T angle, const Vector<T, 3>& axis) {
  static_assert(std::is_floating_point<T>(),
                "rotate3() requires a floating point type");

  Matrix<T, 3, 3> res;

  const auto v = normalize(axis);
  const T x = v[0];
  const T y = v[1];
  const T z = v[2];
  const T c = std::cos(angle);
  const T s = std::sin(angle);

  const T one = 1.0;
  const T omc = one - c;
  const T omcxy = omc * x * y;
  const T omcxz = omc * x * z;
  const T omcyz = omc * y * z;
  const T sx = s * x;
  const T sy = s * y;
  const T sz = s * z;

  res[0][0] = c + omc * x * x;
  res[0][1] = omcxy + sz;
  res[0][2] = omcxz - sy;
  res[1][0] = omcxy - sz;
  res[1][1] = c + omc * y * y;
  res[1][2] = omcyz + sx;
  res[2][0] = omcxz + sy;
  res[2][1] = omcyz - sx;
  res[2][2] = c + omc * z * z;

  return res;
}

/// Matrix rotation (3x3, quaternion).
///
template<class T>
constexpr Matrix<T, 3, 3> rotate3(const Quaternion<T>& qnion) {
  Matrix<T, 3, 3> res;

  const auto v = normalize(qnion.q());
  const T x = v[0];
  const T y = v[1];
  const T z = v[2];
  const T w = v[3];

  const T one = 1.0;
  const T two = 2.0;
  const T xx2 = two * x * x;
  const T xy2 = two * x * y;
  const T xz2 = two * x * z;
  const T xw2 = two * x * w;
  const T yy2 = two * y * y;
  const T yz2 = two * y * z;
  const T yw2 = two * y * w;
  const T zz2 = two * z * z;
  const T zw2 = two * z * w;

  res[0][0] = one - yy2 - zz2;
  res[0][1] = xy2 + zw2;
  res[0][2] = xz2 - yw2;
  res[1][0] = xy2 - zw2;
  res[1][1] = one - xx2 - zz2;
  res[1][2] = yz2 + xw2;
  res[2][0] = xz2 + yw2;
  res[2][1] = yz2 - xw2;
  res[2][2] = one - xx2 - yy2;

  return res;
}

/// Matrix rotation (3x3, x-axis).
///
template<class T>
constexpr Matrix<T, 3, 3> rotate3X(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotate3X() requires a floating point type");

  Matrix<T, 3, 3> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);

  res[0][0] = 1.0;
  res[1][1] = c;
  res[1][2] = s;
  res[2][1] = -s;
  res[2][2] = c;

  return res;
}

/// Matrix rotation (3x3, y-axis).
///
template<class T>
constexpr Matrix<T, 3, 3> rotate3Y(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotate3Y() requires a floating point type");

  Matrix<T, 3, 3> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);

  res[0][0] = c;
  res[0][2] = -s;
  res[1][1] = 1.0;
  res[2][0] = s;
  res[2][2] = c;

  return res;
}

/// Matrix rotation (3x3, z-axis).
///
template<class T>
constexpr Matrix<T, 3, 3> rotate3Z(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotate3Z() requires a floating point type");

  Matrix<T, 3, 3> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);

  res[0][0] = c;
  res[0][1] = s;
  res[1][0] = -s;
  res[1][1] = c;
  res[2][2] = 1.0;

  return res;
}

/// Matrix rotation.
///
template<class T>
constexpr Matrix<T, 4, 4> rotate(T angle, const Vector<T, 3>& axis) {
  static_assert(std::is_floating_point<T>(),
                "rotate() requires a floating point type");

  Matrix<T, 4, 4> res;

  const auto v = normalize(axis);
  const T x = v[0];
  const T y = v[1];
  const T z = v[2];
  const T c = std::cos(angle);
  const T s = std::sin(angle);

  const T one = 1.0;
  const T omc = one - c;
  const T omcxy = omc * x * y;
  const T omcxz = omc * x * z;
  const T omcyz = omc * y * z;
  const T sx = s * x;
  const T sy = s * y;
  const T sz = s * z;

  res[0][0] = c + omc * x * x;
  res[0][1] = omcxy + sz;
  res[0][2] = omcxz - sy;
  res[1][0] = omcxy - sz;
  res[1][1] = c + omc * y * y;
  res[1][2] = omcyz + sx;
  res[2][0] = omcxz + sy;
  res[2][1] = omcyz - sx;
  res[2][2] = c + omc * z * z;
  res[3][3] = one;

  return res;
}

/// Matrix rotation (quaternion).
///
template<class T>
constexpr Matrix<T, 4, 4> rotate(const Quaternion<T>& qnion) {
  Matrix<T, 4, 4> res;

  const auto v = normalize(qnion.q());
  const T x = v[0];
  const T y = v[1];
  const T z = v[2];
  const T w = v[3];

  const T one = 1.0;
  const T two = 2.0;
  const T xx2 = two * x * x;
  const T xy2 = two * x * y;
  const T xz2 = two * x * z;
  const T xw2 = two * x * w;
  const T yy2 = two * y * y;
  const T yz2 = two * y * z;
  const T yw2 = two * y * w;
  const T zz2 = two * z * z;
  const T zw2 = two * z * w;

  res[0][0] = one - yy2 - zz2;
  res[0][1] = xy2 + zw2;
  res[0][2] = xz2 - yw2;
  res[1][0] = xy2 - zw2;
  res[1][1] = one - xx2 - zz2;
  res[1][2] = yz2 + xw2;
  res[2][0] = xz2 + yw2;
  res[2][1] = yz2 - xw2;
  res[2][2] = one - xx2 - yy2;
  res[3][3] = one;

  return res;
}

/// Matrix rotation (x-axis).
///
template<class T>
constexpr Matrix<T, 4, 4> rotateX(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotateX() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);
  const T one = 1.0;

  res[0][0] = one;
  res[1][1] = c;
  res[1][2] = s;
  res[2][1] = -s;
  res[2][2] = c;
  res[3][3] = one;

  return res;
}

/// Matrix rotation (y-axis).
///
template<class T>
constexpr Matrix<T, 4, 4> rotateY(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotateY() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);
  const T one = 1.0;

  res[0][0] = c;
  res[0][2] = -s;
  res[1][1] = one;
  res[2][0] = s;
  res[2][2] = c;
  res[3][3] = one;

  return res;
}

/// Matrix rotation (z-axis).
///
template<class T>
constexpr Matrix<T, 4, 4> rotateZ(T angle) {
  static_assert(std::is_floating_point<T>(),
                "rotateZ() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T c = std::cos(angle);
  const T s = std::sin(angle);
  const T one = 1.0;

  res[0][0] = c;
  res[0][1] = s;
  res[1][0] = -s;
  res[1][1] = c;
  res[2][2] = one;
  res[3][3] = one;

  return res;
}

/// Matrix scale (3x3).
///
template<class T>
constexpr Matrix<T, 3, 3> scale3(T sx, T sy, T sz) {
  Matrix<T, 3, 3> res;
  res[0][0] = sx;
  res[1][1] = sy;
  res[2][2] = sz;
  return res;
}

/// Matrix scale (3x3, vector).
///
template<class T>
constexpr Matrix<T, 3, 3> scale3(const Vector<T, 3>& s) {
  return scale3(s[0], s[1], s[2]);
}

/// Matrix scale.
///
template<class T>
constexpr Matrix<T, 4, 4> scale(T sx, T sy, T sz) {
  Matrix<T, 4, 4> res;
  res[0][0] = sx;
  res[1][1] = sy;
  res[2][2] = sz;
  res[3][3] = 1;
  return res;
}

/// Matrix scale (vector).
///
template<class T>
constexpr Matrix<T, 4, 4> scale(const Vector<T, 3>& s) {
  return scale(s[0], s[1], s[2]);
}

/// Matrix translation.
///
template<class T>
constexpr Matrix<T, 4, 4> translate(T tx, T ty, T tz) {
  auto res = Matrix<T, 4, 4>::identity();
  res[3] = {tx, ty, tz, 1};
  return res;
}

/// Matrix translation (vector).
///
template<class T>
constexpr Matrix<T, 4, 4> translate(const Vector<T, 3>& t) {
  return translate(t[0], t[1], t[2]);
}

/// View matrix.
///
template <class T>
constexpr Matrix<T, 4, 4> lookAt(const Vector<T, 3>& eye,
                                 const Vector<T, 3>& center,
                                 const Vector<T, 3>& up) {

  static_assert(std::is_floating_point<T>(),
                "lookAt() requires a floating point type");

  const auto f = (center - eye).normalize();
  const auto s = cross(f, up).normalize();
  const auto u = cross(f, s);
  const T one = 1.0;
  const T zero = 0.0;

  return {{s[0], u[0], -f[0], zero},
          {s[1], u[1], -f[1], zero},
          {s[2], u[2], -f[2], zero},
          {-dot(s, eye), -dot(u, eye), dot(f, eye), one}};
}

/// Perspective projection matrix.
///
template <class T>
constexpr Matrix<T, 4, 4> perspective(T yFov, T aspect, T zNear, T zFar) {
  static_assert(std::is_floating_point<T>(),
                "perspective() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T one = 1.0;
  const T two = 2.0;
  const T ct = one / std::tan(yFov * 0.5);

  res[0][0] = ct / aspect;
  res[1][1] = ct;
  res[2][2] = (zFar + zNear) / (zNear - zFar);
  res[2][3] = -one;
  res[3][2] = (two * zFar * zNear) / (zNear - zFar);

  return res;
}

/// Infinite perspective projection matrix.
///
template<class T>
constexpr Matrix<T, 4, 4> infPerspective(T yFov, T aspect, T zNear) {
  static_assert(std::is_floating_point<T>(),
                "infPerspective() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T one = 1.0;
  const T two = 2.0;
  const T ct = one / std::tan(yFov * 0.5);

  res[0][0] = ct / aspect;
  res[1][1] = ct;
  res[2][2] = -one;
  res[2][3] = -one;
  res[3][2] = -two * zNear;

  return res;
}

/// Orthographic projection matrix.
///
template <class T>
constexpr Matrix<T, 4, 4> ortho(T xMag, T yMag, T zNear, T zFar) {
  static_assert(std::is_floating_point<T>(),
                "ortho() requires a floating point type");

  Matrix<T, 4, 4> res;

  const T one = 1.0;
  const T two = 2.0;

  res[0][0] = one / xMag;
  res[1][1] = one / yMag;
  res[2][2] = two / (zNear - zFar);
  res[3][2] = (zFar + zNear) / (zNear - zFar);
  res[3][3] = one;

  return res;
}

/// Integer matrices.
///
using Mat2i   = Matrix<int32_t, 2, 2>;
using Mat2x3i = Matrix<int32_t, 2, 3>;
using Mat2x4i = Matrix<int32_t, 2, 4>;
using Mat3x2i = Matrix<int32_t, 3, 2>;
using Mat3i   = Matrix<int32_t, 3, 3>;
using Mat3x4i = Matrix<int32_t, 3, 4>;
using Mat4x2i = Matrix<int32_t, 4, 2>;
using Mat4x3i = Matrix<int32_t, 4, 3>;
using Mat4i   = Matrix<int32_t, 4, 4>;

/// Unsigned integer matrices.
///
using Mat2u   = Matrix<uint32_t, 2, 2>;
using Mat2x3u = Matrix<uint32_t, 2, 3>;
using Mat2x4u = Matrix<uint32_t, 2, 4>;
using Mat3x2u = Matrix<uint32_t, 3, 2>;
using Mat3u   = Matrix<uint32_t, 3, 3>;
using Mat3x4u = Matrix<uint32_t, 3, 4>;
using Mat4x2u = Matrix<uint32_t, 4, 2>;
using Mat4x3u = Matrix<uint32_t, 4, 3>;
using Mat4u   = Matrix<uint32_t, 4, 4>;

/// Single precision matrices.
///
using Mat2f   = Matrix<float, 2, 2>;
using Mat2x3f = Matrix<float, 2, 3>;
using Mat2x4f = Matrix<float, 2, 4>;
using Mat3x2f = Matrix<float, 3, 2>;
using Mat3f   = Matrix<float, 3, 3>;
using Mat3x4f = Matrix<float, 3, 4>;
using Mat4x2f = Matrix<float, 4, 2>;
using Mat4x3f = Matrix<float, 4, 3>;
using Mat4f   = Matrix<float, 4, 4>;

/// Double precision matrices.
///
using Mat2d   = Matrix<double, 2, 2>;
using Mat2x3d = Matrix<double, 2, 3>;
using Mat2x4d = Matrix<double, 2, 4>;
using Mat3x2d = Matrix<double, 3, 2>;
using Mat3d   = Matrix<double, 3, 3>;
using Mat3x4d = Matrix<double, 3, 4>;
using Mat4x2d = Matrix<double, 4, 2>;
using Mat4x3d = Matrix<double, 4, 3>;
using Mat4d   = Matrix<double, 4, 4>;

SG_NS_END

#endif // YF_SG_MATRIX_H
