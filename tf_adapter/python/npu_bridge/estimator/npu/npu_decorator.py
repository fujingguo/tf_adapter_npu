#!/usr/bin/env python
# coding=utf-8

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

import os
from tensorflow.python.keras import backend
from tensorflow.python.ops import state_ops
from tensorflow.python.ops import control_flow_ops
from npu_bridge.hccl import hccl_ops

_keras_graph_key = []

def _broadcast_variables(session):
  op_list = []
  variables = backend._get_variables(backend.get_graph())
  graph_key = backend.get_graph()._graph_key
  candidate_vars = []
  for v in variables:
    if getattr(v, "_keras_initialized", False):
      candidate_vars.append(v)
  if graph_key not in _keras_graph_key and candidate_vars:
    for var in candidate_vars:
      inputs = [var]
      outputs = hccl_ops.broadcast(tensor=inputs, root_rank=0)
      if outputs is not None:
        op_list.append(outputs[0].op)
        op_list.append(state_ops.assign(var, outputs[0]))
    session.run(control_flow_ops.group(op_list))
  _keras_graph_key.append(graph_key)

def npu_keras_var_broadcast_decorator(func):
  def wrapper(*args, **kwargs):
    rank_size  = os.getenv("RANK_SIZE", "1")
    if int(rank_size) <= 1:
      return func(*args, **kwargs)
    else:
      func(*args, **kwargs)
      _broadcast_variables(*args, **kwargs)
  return wrapper

backend._initialize_variables = npu_keras_var_broadcast_decorator(backend._initialize_variables)