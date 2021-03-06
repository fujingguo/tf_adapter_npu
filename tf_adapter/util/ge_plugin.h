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
#ifndef TENSORFLOW_GE_PLUGIN_H_
#define TENSORFLOW_GE_PLUGIN_H_

#include <map>
#include <mutex>
#include <string>
#include "tensorflow/core/platform/types.h"

// Singleton class for manage the relationship between
// tf session and ge session
class GePlugin {
 public:
  static GePlugin *GetInstance();

  void Init(std::map<std::string, std::string> &init_options, const bool is_global = false);

  void Finalize();

  bool IsGlobal();

  std::map<std::string, std::string> GetInitOptions();

 private:
  GePlugin();

  ~GePlugin();

  uint64_t GetFusionTensorSize() const;

  uint32_t device_id_;
  bool isInit_;
  bool isGlobal_;
  std::map<std::string, std::string> init_options_;
  std::mutex mutex_;
};
// } // tensorflow

#endif
