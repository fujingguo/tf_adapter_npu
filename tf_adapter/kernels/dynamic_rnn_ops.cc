/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Copyright (C) 2019-2020. Huawei Technologies Co., Ltd. All rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"

namespace tensorflow {
template<typename T>
class DynamicRnnOP : public OpKernel {
 public:
  explicit DynamicRnnOP(OpKernelConstruction *ctx) : OpKernel(ctx) { LOG(INFO) << "new DynamicRnnOP"; }
  ~DynamicRnnOP() { LOG(INFO) << "del DynamicRnnOP"; }
  void Compute(OpKernelContext *ctx) override { LOG(INFO) << "in DynamicRnnOP"; }
  bool IsExpensive() override { return false; }
};

REGISTER_KERNEL_BUILDER(Name("DynamicRnn").Device(DEVICE_CPU), DynamicRnnOP<float>);
}  // namespace tensorflow