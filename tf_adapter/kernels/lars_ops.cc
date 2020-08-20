/**
* Copyright (C) <2019>  <Huawei Technologies Co., Ltd.>. All Rights Reserved.
* Description : implememt of lars
*/

#ifndef TENSORFLOW_CORE_KERNELS_LARS_OP_H_
#define TENSORFLOW_CORE_KERNELS_LARS_OP_H_

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/bounds_check.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/platform/macros.h"

namespace tensorflow {
template<typename T>
class LarsOp : public OpKernel {
 public:

  explicit LarsOp(OpKernelConstruction *context) : OpKernel(context) {
    LOG(INFO) << "new LarsOp";
  }
  ~LarsOp() {
    LOG(INFO) << "del LarsOp";
  }

  void Compute(OpKernelContext *context) override {
    int input_num = num_inputs();
    LOG(INFO) << "LarsOp: input num " << input_num;
    input_num = ((input_num - 1) / 2);

    for (int j = 0; j < input_num; j++) {
      // Grab the w_input tensor
      const Tensor &w_tensor = context->input(j);
      auto w_input = w_tensor.flat<T>();

      const Tensor &g_tensor = context->input(j + input_num);
      auto g_input = g_tensor.flat<T>();

      // Create an output tensor
      Tensor *output_tensor = NULL;
      OP_REQUIRES_OK(context, context->allocate_output(j, w_tensor.shape(), &output_tensor));
      // handle any data type for w_input and output
      auto output_flat = output_tensor->flat<T>();

      // Set the value of each element
      const int N = w_input.size();
      LOG(INFO) << "LarsOp idx " << j << ", data num " << N;

      auto sum_w = w_input(0);
      auto sum_g = g_input(0);
      for (int i = 1; i < N; i++) {
        auto w = w_input(i);
        sum_w += w;
        LOG(INFO) << "LarsOp w " << w << ", sum_w " << sum_w;

        auto g = g_input(i);
        sum_g += g;
        LOG(INFO) << "LarsOp g " << g << ", sum_g " << sum_g;
      }

      auto w_norm = sqrt(sum_w);
      auto g_norm = sqrt(sum_g);
      auto b = g_norm + w_norm + T(0.00001);

      for (int i = 1; i < N; i++) {
        auto w = w_input(i);
        auto g = g_input(i);
        output_flat(i) = b * (g + w);
      }
    }

    LOG(INFO) << "in LarsOp";
  }
  bool IsExpensive() override { return false; }
};

REGISTER_KERNEL_BUILDER(Name("LARS")
.
Device(DEVICE_CPU)
.TypeConstraint<float>("T"), LarsOp<float>);
}  // namespace tensorflow
#endif  // TENSORFLOW_CORE_KERNELS_LARS_OP_H_
