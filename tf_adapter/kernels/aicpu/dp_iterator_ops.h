/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TENSORFLOW_CORE_KERNELS_DATA_DP_ITERATOR_OPS_H_
#define TENSORFLOW_CORE_KERNELS_DATA_DP_ITERATOR_OPS_H_

#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/kernels/data/captured_function.h"

namespace tensorflow {
namespace data {
class DpMakeIteratorOp : public OpKernel {
 public:
  explicit DpMakeIteratorOp(OpKernelConstruction *ctx) : OpKernel(ctx) {}
  ~DpMakeIteratorOp() override = default;
  void Compute(OpKernelContext *ctx) override;
};

}  // namespace data
}  // namespace tensorflow
#endif  // TENSORFLOW_CORE_KERNELS_DATA_ITERATOR_OPS_H_
