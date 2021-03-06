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

#ifndef TENSORFLOW_GENERATE_REPORT_H_
#define TENSORFLOW_GENERATE_REPORT_H_

#include "tensorflow/core/graph/graph.h"
// Op will be written to json if it can not sink to device during one excute.
namespace tensorflow {
class GenerateReport {
 public:
  struct Details {
    int code;

    std::string message;
  };
  enum class ReasonCode { TypeNoDefine = 1, TypeGray = 2, ScenarioProblems = 3, NotSupport = 4 };

  static GenerateReport *GetInstance();

  Status AddUnSupportedInfo(const std::string &name, const std::string &type, const Details &infos);

  Status AddUnSupportedInfo(const Node *node, Details &infos);

  Status SaveUnsupportedInfo();

  ~GenerateReport();

 private:
  GenerateReport();
  struct UnSupportedInfo {
    std::string name;
    std::string type;
    bool is_support = 0;
    Details info_details;
  };
  std::map<std::string, UnSupportedInfo> check_info_map_;
};
}  // namespace tensorflow

#endif
