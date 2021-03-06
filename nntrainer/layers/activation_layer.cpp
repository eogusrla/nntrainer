// SPDX-License-Identifier: Apache-2.0
/**
 * Copyright (C) 2020 Jihoon Lee <jhoon.it.lee@samsung.com>
 *
 * @file   activation_layer.cpp
 * @date   17 June 2020
 * @see    https://github.com/nnstreamer/nntrainer
 * @author Jihoon Lee <jhoon.it.lee@samsung.com>
 * @bug    No known bugs except for NYI items
 * @brief  This is Activation Layer Class for Neural Network
 *
 */

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <vector>

#include <activation_layer.h>
#include <blas_interface.h>
#include <layer_internal.h>
#include <lazy_tensor.h>
#include <nntrainer_error.h>
#include <nntrainer_log.h>
#include <optimizer_internal.h>
#include <parse_util.h>
#include <tensor.h>
#include <util_func.h>

namespace nntrainer {

const std::string ActivationLayer::type = "activation";

/**
 * @brief     Initialize the layer
 *
 * @retval #ML_ERROR_NONE Successful.
 * @retval #ML_ERROR_INVALID_PARAMETER invalid parameter.
 */
int ActivationLayer::initialize(Manager &manager) {

  output_dim = input_dim;

  return ML_ERROR_NONE;
}

void ActivationLayer::forwarding() {
  Tensor &hidden_ = net_hidden[0]->getVariableRef();
  /// @note @a _act_fn is expected to work out of place and not modify @a input
  _act_fn(net_input[0]->getVariableRef(), hidden_);
}

void ActivationLayer::calcDerivative() {
  Tensor &deriv = net_hidden[0]->getGradientRef();
  Tensor &ret = net_input[0]->getGradientRef();
  Tensor &in = net_hidden[0]->getVariableRef();

  ret = _act_prime_fn(in, ret, deriv);
}

int ActivationLayer::setActivation(
  std::function<Tensor &(Tensor const &, Tensor &)> const &activation_fn,
  std::function<Tensor &(Tensor &, Tensor &, Tensor const &)> const
    &activation_prime_fn) {
  _act_fn = activation_fn;
  _act_prime_fn = activation_prime_fn;

  return ML_ERROR_NONE;
}

int ActivationLayer::setActivation(
  std::function<Tensor &(Tensor const &, Tensor &)> const &activation_fn,
  std::function<Tensor &(Tensor &, Tensor &)> const &activation_prime_fn) {
  _act_fn = activation_fn;
  _act_prime_fn = [activation_prime_fn](Tensor &x, Tensor &ret_derivative,
                                        Tensor const &derivative) -> Tensor & {
    x = activation_prime_fn(x, x);
    ret_derivative = derivative.multiply(x, ret_derivative);

    return ret_derivative;
  };

  return ML_ERROR_NONE;
}

int ActivationLayer::setActivation(
  std::function<float(float const)> const &activation_fn,
  std::function<float(float const)> const &activation_prime_fn) {
  _act_fn = [activation_fn](Tensor const &x, Tensor &hidden) -> Tensor & {
    return x.apply(activation_fn, hidden);
  };
  _act_prime_fn = [activation_prime_fn](Tensor &x, Tensor &ret_derivative,
                                        Tensor const &derivative) -> Tensor & {
    x = x.apply(activation_prime_fn, x);
    ret_derivative = derivative.multiply(x, ret_derivative);

    return ret_derivative;
  };

  return ML_ERROR_NONE;
}

/**
 * @brief setActivation by preset ActivationType
 *
 * @param[in] ActivationType ActivationType ActivationType to be set
 */
void ActivationLayer::setActivation(ActivationType acti_type) {
  Layer::setActivation(acti_type);

  switch (acti_type) {
  case ActivationType::ACT_TANH:
    this->setActivation(tanhFloat, tanhPrime);
    break;
  case ActivationType::ACT_SIGMOID:
    this->setActivation(sigmoid, sigmoidPrime);
    break;
  case ActivationType::ACT_SOFTMAX:
    this->setActivation(softmax, softmaxPrime);
    break;
  case ActivationType::ACT_RELU:
    this->setActivation(relu, reluPrime);
    break;
  case ActivationType::ACT_NONE:
    this->setActivation(no_op, no_op_prime);
    break;
  case ActivationType::ACT_UNKNOWN:
  default:
    throw std::runtime_error("Error: Not Supported Activation Type");
  }
}

Tensor &ActivationLayer::softmax(Tensor const &t, Tensor &output) {
  /**
   * shiftx_logit = logit - max_batch(logit)
   * softmax = exp(shiftx_logit) / (sum(exp(shiftx_logit)))
   */
  unsigned int batch = t.batch();
  float *dp;
  float *rp;

  Tensor divisor = t.clone();

  dp = divisor.getData();
  unsigned int feat_len = t.getDim().getFeatureLen();

  for (unsigned int k = 0; k < batch; k++) {
    int index = k * feat_len;
    // find max and subtract it
    float m = *std::max_element(dp + index, dp + index + feat_len);

    Tensor tmp = Tensor(1, 1, 1, feat_len);
    tmp.setValue(m);
    saxpy(feat_len, -1, tmp.getData(), 1, dp + index, 1);
  }

  // take exp
  output = divisor.apply(exp_util, output);
  rp = output.getData();
  // take sum over batch
  Tensor sum = output.sum_by_batch();

  for (unsigned int k = 0; k < batch; k++) {
    int index = k * feat_len;
    std::transform(rp + index, rp + index + feat_len, rp + index,
                   std::bind(std::divides<float>(), std::placeholders::_1,
                             sum.getValue(k, 0, 0, 0)));
  }

  return output;
}

Tensor &ActivationLayer::softmaxPrime(Tensor const &x, Tensor &output,
                                      Tensor const &derivative) {
  unsigned int batch = x.batch();
  unsigned int channel = x.channel();
  unsigned int height = x.height();
  unsigned int width = x.width();
  bool is_derivative = true;

  if (output.uninitialized())
    output = Tensor(x.getDim());

  const float *xp = x.getData();
  const float *d = derivative.getData();
  float *pp = output.getData();

  /** @todo update default tensorDim to be 0 and not 1 */
  if (derivative.getDim() == TensorDim()) {
    is_derivative = false;
  }

  for (unsigned int k = 0; k < batch; ++k) {
    int K = k * channel * height * width;
    for (unsigned int c = 0; c < channel; ++c) {
      int C = K + c * height * width;
      for (unsigned int i = 0; i < height; ++i) {
        int I = C + i * width;
        for (unsigned int j = 0; j < width; ++j) {
          float sum = 0.0f;
          for (unsigned int l = 0; l < width; ++l) {
            float val;
            if (j == l) {
              val = xp[I + l] * (1.0f - xp[I + j]);
            } else {
              val = -xp[I + l] * xp[I + j];
            }
            if (is_derivative)
              val *= d[I + l];
            sum += val;
          }
          pp[I + j] = sum;
        }
      }
    }
  }
  return output;
}

float ActivationLayer::sigmoid(float x) { return 1.0f / (1.0f + exp_util(-x)); }

float ActivationLayer::sigmoidPrime(float x) {
  // float sprime = sigmoid(x);
  return x * (1.0f - x);
}

float ActivationLayer::tanhFloat(float x) { return (float)tanh(x); }

float ActivationLayer::tanhPrime(float x) {
  // float th = (float)tanh(x);
  return 1.0f - x * x;
}

float ActivationLayer::relu(float x) {
  if (x <= 0.0f) {
    return 0.0f;
  } else {
    return x;
  }
}

float ActivationLayer::reluPrime(float x) {
  if (x <= 0.0f) {
    return 0.0f;
  } else {
    return 1.0f;
  }
}

float ActivationLayer::no_op(float x) { return x; }

float ActivationLayer::no_op_prime(float x) { return 1.0f; }
}; // namespace nntrainer
