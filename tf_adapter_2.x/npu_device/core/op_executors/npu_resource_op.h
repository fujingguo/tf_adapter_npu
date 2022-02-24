/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
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

#ifndef NPU_DEVICE_CORE_OP_EXECUTORS_NPU_RESOURCE_OP_H
#define NPU_DEVICE_CORE_OP_EXECUTORS_NPU_RESOURCE_OP_H

#include "npu_op_executor.h"
#include "npu_concrete_graph.h"

namespace npu {
class NpuResourceOp : public OpExecutor {
  using HashKey = uint64_t;

 public:
  NpuResourceOp(const tensorflow::OpRegistrationData *op_spec, const tensorflow::NodeDef &ndef,
                TensorShapes input_shapes);

  const std::string &Type() const override {
    const static std::string kType = "NpuResourceOp";
    return kType;
  }

  std::string AttachedDebugString() const override;

  void RunImpl(TFE_Context *context, NpuDevice *device, int num_inputs, TFE_TensorHandle **inputs, int num_outputs,
               TFE_TensorHandle **outputs, TF_Status *status) const override;

 private:
  static HashKey Hash(const std::vector<tensorflow::ResourceHandle> &handles);

  void GetFuncSpec(const std::vector<tensorflow::ResourceHandle> &handles,
                   std::shared_ptr<NpuConcreteGraph> *spec) const;

  void CacheFuncSpec(const std::vector<tensorflow::ResourceHandle> &handles,
                     std::shared_ptr<NpuConcreteGraph> spec) const;

  tensorflow::mutex mutable shared_lock;
  std::map<HashKey, std::shared_ptr<NpuConcreteGraph>> mutable handles_to_specs_ TF_GUARDED_BY(shared_lock);
};
}  // namespace npu

#endif  // NPU_DEVICE_CORE_OP_EXECUTORS_NPU_RESOURCE_OP_H
