# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

from npu_bridge.helper.helper import npu_bridge_handle
from npu_bridge.helper.helper import version as __version__
from npu_bridge.helper import helper
from npu_bridge.estimator.npu import npu_estimator
from npu_bridge.estimator.npu import npu_decorator
from npu_bridge.hccl import hccl_ops
from npu_bridge.estimator.npu.npu_plugin import npu_close
import atexit
atexit.register(npu_close)
__all__ = [_s for _s in dir() if not _s.startswith('_')]