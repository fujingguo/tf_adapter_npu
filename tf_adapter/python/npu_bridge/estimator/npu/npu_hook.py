#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2019-2021. All rights reserved.
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

"""Definition of NPU hooks"""

import os
import threading
import time
from six.moves import queue as Queue

import tensorflow as tf
from tensorflow.core.protobuf import config_pb2
from tensorflow.python.ops import summary_ops_v2 as contrib_summary
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training import session_run_hook
from tensorflow.python.training import basic_session_run_hooks

from npu_bridge.estimator import npu_ops
from npu_bridge.hccl import hccl_ops
from npu_bridge.estimator.npu import util

# Constant
_BATCH_SIZE_KEY = 'batch_size'
_RESERVED_PARAMS_KEYS = [_BATCH_SIZE_KEY]

_NPU_RUNCONFIG = 'npu_runconfig'
_ITERATIONS_PER_LOOP_VAR = 'iterations_per_loop'
_LOOP_COND_VAR = 'loop_cond'
_CONST_ZERO = 'zero'
_CONST_ONE = 'one'

util.register_func(_ITERATIONS_PER_LOOP_VAR)
util.register_func(_LOOP_COND_VAR)
util.register_func(_CONST_ZERO)
util.register_func(_CONST_ONE)


class NPUShutDownHook(session_run_hook.SessionRunHook):
    """Hook to shutdown the system."""

    def __init__(self, scaffold=None):
        super(NPUShutDownHook, self).__init__()
        self._scaffold = scaffold
        self._shutdown_npu_op = None

    def begin(self):
        """Call when session hook begins"""
        if not self._shutdown_npu_op or self._shutdown_npu_op.graph != tf.get_default_graph():
            self._shutdown_npu_op = npu_ops.NPUShutdown()

    def end(self, session):
        """Call at the end of session hook"""
        logging.info("NPUShutDownHook run...")
        session.run(self._shutdown_npu_op)


class NPUBroadcastGlobalVariablesHook(session_run_hook.SessionRunHook):
    """Broadcasts initial variable states from rank 0 to all other processes.

    This is necessary to ensure consistent initialization of all workers when
    training is started with random weights or restored from a checkpoint.

    """

    def __init__(self, root_rank=None, index=None):
        """Construct a new NPUBroadcastGlobalVariablesHook that will broadcast all
        global variables from root rank to all other processes during initialization.

        Args:
          root_rank:
            Rank that will send data, other ranks will receive data.
          index:
            Current rand id.
        """
        self._root_rank = root_rank
        self._index = index
        self._bcast_op = None
        rank_size = os.getenv('RANK_SIZE', "1")
        if rank_size.isdigit():
            self._rank_size = int(rank_size)
        else:
            self._rank_size = 1

    def begin(self):
        """Call when session hook begins"""
        if (not self._bcast_op or self._bcast_op.graph != tf.get_default_graph()) and self._rank_size > 1:
            self._bcast_op = broadcast_global_variables(self._root_rank)

    def after_create_session(self, session, coord):
        """Call when session is created"""
        if self._rank_size > 1:
            logging.info("NPUBroadcastGlobalVariablesHook run...")
            session.run(self._bcast_op)


class NPUCheckpointSaverHook(basic_session_run_hooks.CheckpointSaverHook):
    """Hook to save training checkpoints"""
    def __init__(self,
                 checkpoint_dir,
                 save_secs=None,
                 save_steps=None,
                 saver=None,
                 checkpoint_basename="model.ckpt",
                 scaffold=None,
                 listeners=None):
        super(NPUCheckpointSaverHook, self).__init__(
            checkpoint_dir=checkpoint_dir,
            save_secs=save_secs,
            save_steps=save_steps,
            saver=saver,
            checkpoint_basename=checkpoint_basename,
            scaffold=scaffold,
            listeners=listeners)

    def after_run(self, run_context, run_values):
        """Call after session has run"""
        global_step = run_context.session.run(self._global_step_tensor)
        logging.info("global_step..." + str(global_step))
        super().after_run(run_context, run_values)

    def end(self, session):
        """Call at the end of session hook"""
        logging.info("NPUCheckpointSaverHook end...")
        super().end(session)


class SetIterationsVarHook(session_run_hook.SessionRunHook):
    """Hook to set iteration variables."""
    def __init__(self, iterations_per_loop=None):
        self._iterations_per_loop = iterations_per_loop
        self._iterations_per_loop_var = None
        self._const_one = None
        self._const_zero = None
        self._loop_cond_var = None

    def begin(self):
        """Call when session hook begins"""
        self._iterations_per_loop_var = util.create_or_get_var(_ITERATIONS_PER_LOOP_VAR)
        self._loop_cond_var = util.create_or_get_var(_LOOP_COND_VAR)
        self._const_zero = util.create_or_get_var(_CONST_ZERO)
        self._const_one = util.create_or_get_var(_CONST_ONE)

    def after_create_session(self, session, coord):
        """Call when session is created"""
        self._iterations_per_loop_var.load(self._iterations_per_loop - 1, session=session)
        self._loop_cond_var.load(0, session=session)
        self._const_zero.load(0, session=session)
        self._const_one.load(1, session=session)
        print("load iterations_per_loop value -----------")
        print(session.run(self._iterations_per_loop_var))


def broadcast_global_variables(root_rank):
    """Broadcasts all global variables from root rank to all other processes.
    Arguments:
        root_rank: rank of the process from which global variables will be broadcasted
        to all other processes.
    """
    op_list = []
    for var in tf.trainable_variables():
        # the input and out tensor of HCOMBroadcast interface are list
        inputs = [var]
        outputs = hccl_ops.broadcast(tensor=inputs, root_rank=root_rank)
        if outputs is not None:
            op_list.append(outputs[0].op)
            op_list.append(tf.assign(var, outputs[0]))

    return tf.group(op_list)


class _SIGNAL:
    STOP = -1


class _OpQueueContext:
    """Manages work queue and thread for a infeed/outfeed thread."""
    def __init__(self, name, target, args):
        self._name = name
        self._queue = Queue.Queue()
        args = (self,) + args
        self._thread = threading.Thread(name=name, target=target, args=args)
        self._thread.daemon = False
        self._thread.start()

    def stop(self):
        """Stop work queue"""
        self._queue.put(_SIGNAL.STOP)


class NPULogOutfeedSessionHook(session_run_hook.SessionRunHook):
    """Hook to logout feed session"""
    def __init__(self, output_stream):
        self._output_stream = output_stream
        self._stopped = False
        self._dequeue_ops = None
        self._outfeed_controller = None
        self._finalize_ops = None

    def begin(self):
        """Call when session hook begins"""
        self._finalize_ops = []
        outfeed_log_tensors = npu_ops.outfeed_dequeue_op(
                channel_name="_npu_log",
                output_types=[tf.string],
                output_shapes=[()])
        self._dequeue_ops = tf.print(outfeed_log_tensors, output_stream=self._output_stream)

    def _run_outfeed(self, queue_ctx, session):
        logging.info('Starting log outfeed thread controller.')
        while True:
            try:
                session.run(self._dequeue_ops)
            except Exception:
                logging.info('Log outfeed thread finished')
                break

    def after_create_session(self, session, coord):
        """Call when session is created"""
        self._outfeed_controller = _OpQueueContext(
            name='LogOutfeedController', target=self._run_outfeed, args=(session,))

    def end(self, session):
        """Call at the end of session hook"""
        if not self._stopped:
            self._stopped = True
            if self._finalize_ops:
                session.run(self._finalize_ops)


class NPUInfeedOutfeedSessionHook(session_run_hook.SessionRunHook):
    """Hook to in feed out feed session"""
    def __init__(self,
                 dequeue_ops,
                 channel_name):
        self._dequeue_ops = dequeue_ops
        self._channel_name = channel_name
        self._finished = False
        self._stopped = False
        self._init_ops = None
        self._outfeed_controller = None
        self._finalize_ops = None

    def begin(self):
        """Call when session hook begins"""
        self._init_ops = []
        self._finalize_ops = []

        summary_writer_init_ops = contrib_summary.summary_writer_initializer_op()
        self._init_ops.extend(summary_writer_init_ops)
        # Get all the writer resources from the initializer, so we know what to flush.
        for op in summary_writer_init_ops:
            self._finalize_ops.append(contrib_summary.flush(writer=op.inputs[0]))

    def _run_outfeed(self, queue_ctx, session):
        logging.info('Starting outfeed thread controller.')
        while True:
            try:
                session.run(self._dequeue_ops)
            except Exception:
                logging.info('summary outfeed thread finished')
                break
        logging.info('Outfeed thread finished, shutting down')

    def after_create_session(self, session, coord):
        """Call when session is created"""
        logging.info('Init NPU system')
        start = time.time()
        session.run(self._init_ops,
                    options=config_pb2.RunOptions(timeout_in_ms=5 * 60 * 1000))
        logging.debug('Initialized NPU in %d seconds', time.time() - start)

        self._outfeed_controller = _OpQueueContext(
            name='OutfeedController', target=self._run_outfeed, args=(session,))
        logging.info('Add log output coordinate thread to coord')

    def end(self, session):
        """Call at the end of session hook"""
        self._finished = True

        logging.info('Shutdown NPU system.')
        if not self._stopped:
            self._stopped = True
            if self._finalize_ops:
                session.run(self._finalize_ops)


class NPUOutputTensorHook(basic_session_run_hooks.LoggingTensorHook):
    """call output_fn to print tensors every N steps or at end."""

    def __init__(self, tensors,
                 dependencies=None,
                 output_fn=None,
                 output_every_n_steps=0
                 ):
        """Initializes a `NPUOutputTensorHook`.

        Args:
            tensors: `dict` that maps string-valued tags to tensors/tensor names,
                or `iterable` of tensors/tensor names.
            dependencies: control edges.
            output_fn: A callable, uses __call__ to print tensors
            output_every_n_steps: `int`, print the values of `tensors` once every N local
            steps taken on the current worker.

        """
        self._tensors = None
        self._output_fn = output_fn
        self._output_every_n_steps = output_every_n_steps
        self._output_list = []
        self._iter_count = 0
        if tensors is not None:
            if dependencies is not None:
                if not isinstance(dependencies, (tuple, list)):
                    dependencies = [dependencies]

                with tf.control_dependencies(dependencies):
                    self._tensors = {k: tf.identity(v) for k, v in tensors.items()}
            else:
                self._tensors = tensors

            super(NPUOutputTensorHook, self).__init__(self._tensors, every_n_iter=1 << 31)

    def begin(self):
        """Call when session hook begins"""
        logging.info("NPUOutputTensorHook begin...")
        if self._tensors is not None:
            super(NPUOutputTensorHook, self).begin()

    def before_run(self, run_context):
        """Call before session will run"""
        logging.info("NPUOutputTensorHook before_run...", self._tensors)
        if self._tensors is not None:
            return tf.train.SessionRunArgs(self._current_tensors)

    def after_run(self, run_context, run_values):
        """Call after session has run"""
        logging.info("NPUOutputTensorHook after_run...", run_values.results)
        _ = run_context
        if self._tensors is not None:
            self._stash_outputs(run_values.results)

        self._iter_count += 1
        if self._iter_count % self._output_every_n_steps == 0:
            self._call_output_fn()

    def end(self, session):
        """Call at the end of session hook"""
        logging.info("NPUOutputTensorHook end...")
        if self._output_list is not None and self._output_list:
            self._call_output_fn()

    def _stash_outputs(self, tensor_values):
        self._output_list.append({tag: tensor_values[tag] for tag in self._tag_order})

    def _call_output_fn(self):
        self._output_fn.__call__(self._output_list)
        del self._output_list[:]
