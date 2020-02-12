/**
 * Copyright (C) 2019 Samsung Electronics Co., Ltd. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * @file	tensor.cpp
 * @date	04 December 2019
 * @brief	This is Tensor class for calculation
 * @see		https://github.sec.samsung.net/jijoong-moon/Transfer-Learning.git
 * @author	Jijoong Moon <jijoong.moon@samsung.com>
 * @bug		No known bugs except for NYI items
 *
 */

#include "include/tensor.h"
#include <assert.h>
#include <stdio.h>
#include <cstring>
#include <sstream>

Tensor::Tensor(int height, int width) {
  this->height = height;
  this->width = width;
  this->batch = 1;
  this->dim = 2;
  this->len = height * width * batch;
  this->data = std::vector<float>(len);
  setZero();
}

Tensor::Tensor(int batch, int height, int width) {
  this->height = height;
  this->width = width;
  this->batch = batch;
  this->dim = 3;
  this->len = height * width * batch;
  this->data = std::vector<float>(len);
  setZero();
}

float Tensor::getValue(int batch, int h, int w) {
  return this->data[batch * height * width + h * width + w];
}

void Tensor::setValue(int batch, int h, int w, float value) {
  this->data[batch * height * width + h * width + w] = value;
}

Tensor::Tensor(std::vector<std::vector<float>> const &d) {
  assert(d.size() != 0);
  this->height = d.size();
  this->width = d[0].size();
  this->batch = 1;
  this->dim = 2;
  this->len = height * width * batch;
  this->data = std::vector<float>(len);

  for (int j = 0; j < height; ++j)
    for (int k = 0; k < width; ++k)
      this->setValue(0, j, k, d[j][k]);
}

Tensor::Tensor(std::vector<std::vector<std::vector<float>>> const &d) {
  assert(d.size() != 0 && d[0].size() != 0);
  this->batch = d.size();
  this->height = d[0].size();
  this->width = d[0][0].size();
  this->dim = 3;
  this->len = this->batch * this->height * this->width;
  this->data = std::vector<float>(len);

  for (int i = 0; i < this->batch; ++i)
    for (int j = 0; j < this->height; ++j)
      for (int k = 0; k < this->width; ++k)
        this->setValue(i, j, k, d[i][j][k]);
}

Tensor Tensor::multiply(float const &value) {
  Tensor result(batch, height, width);
#ifdef USE_BLAS
  memset(result.data.data(), 0, sizeof(float) * result.len);
  cblas_saxpy(this->len, value, this->data.data(), 1, result.data.data(), 1);
#else
  for (int k = 0; k < len; ++k) {
    result.data[k] = data[k] * value;
  }
#endif
  return result;
}

Tensor Tensor::divide(float const &value) {
  Tensor result(batch, height, width);
#ifdef USE_BLAS
  memset(result.data.data(), 0, sizeof(float) * result.len);
  cblas_saxpy(this->len, 1.0 / value, this->data.data(), 1, result.data.data(), 1);
#else
  for (int k = 0; k < len; ++k) {
    result.data[k] = data[k] / value;
  }
#endif
  return result;
}

Tensor Tensor::add(float const &value) {
  Tensor result(batch, height, width);
#ifdef USE_BLAS
  cblas_scopy(this->len, this->data.data(), 1, result.data.data(), 1);
  Tensor tmp(batch, height, width);
  for (int i = 0; i < tmp.len; ++i)
    tmp.data[i] = 1.0;
  cblas_saxpy(this->len, value, tmp.data.data(), 1, result.data.data(), 1);
#else
  for (int k = 0; k < batch; ++k) {
    result.data[k] = data[k] + value;
  }
#endif

  return result;
}

Tensor Tensor::add(Tensor const &m) const {
  assert(height == m.height && width == m.width);

  Tensor result(batch, height, width);
#ifdef USE_BLAS
  cblas_scopy(this->len, this->data.data(), 1, result.data.data(), 1);
  int size = this->width * this->height;
  if (m.batch == 1) {
    for (int k = 0; k < batch; ++k) {
      cblas_saxpy(size, 1.0, m.data.data(), 1, &(result.data.data()[k * size]), 1);
    }
  } else {
    cblas_saxpy(this->len, 1.0, m.data.data(), 1, result.data.data(), 1);
  }
#else
  int i, j, k;
  if (m.batch == 1) {
    for (k = 0; k < batch; ++k) {
      for (i = 0; i < m.len; ++i) {
        j = k * m.len;
        result.data[j + i] = data[j + i] + m.data[i];
      }
    }
  } else {
    for (k = 0; k < len; ++k) {
      result.data[k] = data[k] + m.data[k];
    }
  }
#endif

  return result;
}

Tensor Tensor::subtract(Tensor const &m) const {
  assert(height == m.height && width == m.width);
  Tensor result(batch, height, width);

#ifdef USE_BLAS
  cblas_scopy(this->len, this->data.data(), 1, result.data.data(), 1);
  int size = this->width * this->height;
  float alpha = -1.0;

  if (m.batch == 1) {
    for (int k = 0; k < batch; ++k) {
      cblas_saxpy(size, alpha, m.data.data(), 1, &(result.data.data()[k * size]), 1);
    }
  } else {
    assert(batch == m.batch);
    cblas_saxpy(this->len, alpha, m.data.data(), 1, result.data.data(), 1);
  }
#else
  int i, j, k;
  if (m.batch == 1) {
    for (k = 0; k < batch; ++k) {
      for (i = 0; i < m.len; ++i) {
        j = k * m.len;
        result.data[j + i] = data[j + i] - m.data[i];
      }
    }
  } else {
    for (k = 0; k < len; ++k) {
      result.data[k] = data[k] - m.data[k];
    }
  }
#endif
  return result;
}

Tensor Tensor::multiply(Tensor const &m) const {
  assert(height == m.height && width == m.width);
  Tensor result(batch, height, width);

  int end = this->len / 4;
  int e = width * height / 4;
  int i;
  if (m.batch == 1) {
    for (int k = 0; k < batch; ++k) {
      int b = k * width * height;
      for (i = 0; i < e * 4; i += 4) {
        result.data[b + i + 0] = this->data[b + i + 0] * m.data[i + 0];
        result.data[b + i + 1] = this->data[b + i + 1] * m.data[i + 1];
        result.data[b + i + 2] = this->data[b + i + 2] * m.data[i + 2];
        result.data[b + i + 3] = this->data[b + i + 3] * m.data[i + 3];
      }
      for (int j = i; j < width * height; j++)
        result.data[b + j] = this->data[b + j] * m.data[j];
    }
  } else {
    for (i = 0; i < end * 4; i += 4) {
      result.data[i + 0] = this->data[i + 0] * m.data[i + 0];
      result.data[i + 1] = this->data[i + 1] * m.data[i + 1];
      result.data[i + 2] = this->data[i + 2] * m.data[i + 2];
      result.data[i + 3] = this->data[i + 3] * m.data[i + 3];
    }
    for (int j = i; j < len; ++j)
      result.data[j] = this->data[j] * m.data[j];
  }

  return result;
}

Tensor Tensor::divide(Tensor const &m) const {
  assert(height == m.height && width == m.width);
  Tensor result(batch, height, width);

  int end = this->len / 4;
  int e = width * height / 4;
  int i;

  if (m.batch == 1) {
    for (int k = 0; k < batch; ++k) {
      int b = k * width * height;
      for (i = 0; i < e * 4; i += 4) {
        result.data[b + i + 0] = this->data[b + i + 0] / m.data[i + 0];
        result.data[b + i + 1] = this->data[b + i + 1] / m.data[i + 1];
        result.data[b + i + 2] = this->data[b + i + 2] / m.data[i + 2];
        result.data[b + i + 3] = this->data[b + i + 3] / m.data[i + 3];
      }
      for (int j = i - 1; j < width * height; ++j)
        result.data[b + j] = this->data[b + j] / m.data[j];
    }
  } else {
    for (i = 0; i < end * 4; i += 4) {
      result.data[i + 0] = this->data[i + 0] / m.data[i + 0];
      result.data[i + 1] = this->data[i + 1] / m.data[i + 1];
      result.data[i + 2] = this->data[i + 2] / m.data[i + 2];
      result.data[i + 3] = this->data[i + 3] / m.data[i + 3];
    }
    for (int j = i - 1; j < len; ++j)
      result.data[j] = this->data[j] / m.data[j];
  }

  return result;
}

/**
 * This is to sum the Tensor data according to the batch.
 * Therefore the result has M(batch, 1, 1) dimension.
 */
Tensor Tensor::sum() const {
  int k;
  Tensor ret(batch, 1, 1);
#ifdef USE_BLAS
  for (k = 0; k < batch; ++k)
    ret.data[k] = cblas_sasum(width * height, &(data.data()[k * width * height]), 1);
#else
  int i;
  for (k = 0; k < batch; ++k) {
    int id = k * width * height;
    ret.data[id] = 0.0;
    for (i = 0; i < height * width; ++i) {
      ret.data[id] += data[id + i];
    }
  }
#endif

  return ret;
}

/**
 * If the batch sizeo of m is one, the it is reused for
 * every calculation along with batch
 */
Tensor Tensor::dot(Tensor const &m) const {
  assert(width == m.height);
  int mwidth = m.width;
  Tensor result(batch, height, mwidth);

#ifdef USE_BLAS
  float alpha_dgemm = 1.0;
  float beta_dgemm = 1.0;
  if (m.batch == 1) {
    for (int k = 0; k < batch; k++) {
      int i = k * width * height;
      int ii = k * height * mwidth;
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, height, mwidth, width, alpha_dgemm, &(data.data()[i]),
                  width, m.data.data(), mwidth, beta_dgemm, &(result.data.data()[ii]), mwidth);
    }
  } else {
    for (int k = 0; k < batch; k++) {
      int i = k * width * height;
      int j = k * m.width * m.height;
      int ii = k * height * mwidth;

      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, height, mwidth, width, alpha_dgemm, &(data.data()[i]),
                  width, &(m.data.data()[j]), mwidth, beta_dgemm, &(result.data.data()[ii]), mwidth);
    }
  }
#else
  float w = 0.0;
  int i, j, k, h;
  if (m.batch == 1) {
    for (k = 0; k < batch; ++k) {
      for (i = 0; i < height; ++i) {
        for (j = 0; j < mwidth; ++j) {
          for (h = 0; h < width; ++h) {
            w += data[k * height * width + i * width + h] * m.data[h * mwidth + j];
          }
          result.data[k * height * mwidth + i * mwidth + j] = w;
          w = 0.0;
        }
      }
    }
  } else {
    for (k = 0; k < batch; k++) {
      for (i = 0; i < height; i++) {
        for (j = 0; j < mwidth; j++) {
          for (h = 0; h < width; h++) {
            w += data[k * height * width + i * width + h] * m.data[k * width * mwidth + h * mwidth + j];
          }
          result.data[k * height * mwidth + i * mwidth + j] = w;
          w = 0.0;
        }
      }
    }
  }
#endif

  return result;
}

Tensor Tensor::transpose() const {
  Tensor result(batch, width, height);
  int i, j, k;
  for (k = 0; k < batch; ++k) {
    int b = k * width * height;
    for (i = 0; i < width; ++i) {
      for (j = 0; j < height; ++j) {
        result.data[b + i * height + j] = data[b + j * width + i];
      }
    }
  }
  return result;
}

Tensor Tensor::applyFunction(float (*function)(float)) const {
  Tensor result(batch, height, width);
  int i;

  for (i = 0; i < this->len; ++i)
    result.data[i] = (*function)(data[i]);

  return result;
}

void Tensor::print(std::ostream &out) const {
  int i, j, k;
  std::stringstream ss;
  for (k = 0; k < batch; k++) {
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        out << data[k * width * height + i * width + j] << " ";
      }
      out << std::endl;
    }
    out << std::endl;
  }
}

std::ostream &operator<<(std::ostream &out, Tensor const &m) {
  m.print(out);
  return out;
}

Tensor &Tensor::copy(const Tensor &from) {
  if (this != &from && from.len != 0) {
    height = from.height;
    width = from.width;
    batch = from.batch;
#ifdef USE_BLAS
    cblas_scopy(this->len, from.data.data(), 1, this->data.data(), 1);
#else
    for (int i = 0; i < len; ++i)
      data[i] = from.data[i];
#endif
  }

  return *this;
}

/**
 * This generate one dimension vector has the every element in Tensor
 */
std::vector<float> Tensor::Mat2Vec() {
  std::vector<float> ret;

  for (int i = 0; i < this->len; i++)
    ret.push_back(data[i]);

  return ret;
}

void Tensor::save(std::ofstream &file) {
  for (int i = 0; i < this->len; i++)
    file.write((char *)&data[i], sizeof(float));
}

void Tensor::read(std::ifstream &file) {
  for (int i = 0; i < this->len; i++)
    file.read((char *)&data[i], sizeof(float));
}

/**
 * This calculates average value according to the batch direction.
 * That is the why it has (1, height, width) dimension.
 */
Tensor Tensor::average() const {
  if (batch == 1)
    return *this;

  Tensor result(1, height, width);
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      result.data[i * width + j] = 0.0;
      for (int k = 0; k < batch; k++) {
        result.data[i * width + j] += data[k * width * height + i * width + j];
      }
      result.data[i * width + j] = result.data[i * width + j] / (float)batch;
    }
  }
  return result;
}

void Tensor::setZero() { memset(this->data.data(), 0, sizeof(float) * this->len); }

Tensor Tensor::softmax() const {
  Tensor result(batch, height, width);
  Tensor divisor(batch, height, 1);

  divisor.setZero();

  for (int k = 0; k < batch; k++) {
    int index = k * height;
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        divisor.data[index + i] += exp(this->data[k * height * width + i * width + j]);
      }
    }
  }

  for (int k = 0; k < batch; k++) {
    int index = k * height;
    for (int i = 0; i < height; i++) {
      for (int j = 0; j < width; j++) {
        int id = k * height * width + i * width + j;
        result.data[id] = exp(this->data[id]) / divisor.data[index + i];
      }
    }
  }

  return result;
}

int Tensor::argmax() {
  int index = 0;
  float maximum = 0.0;
  for (int i = 0; i < len; i++) {
    if (this->data[i] > maximum) {
      maximum = this->data[i];
      index = i;
    }
  }
  return index;
}
