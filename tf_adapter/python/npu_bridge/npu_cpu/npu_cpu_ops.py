#!/usr/bin/env python3
# -*- coding: utf-8 -*-
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

from tensorflow.contrib.util import loader
from tensorflow.python.framework import load_library
from tensorflow.python.framework import ops
from tensorflow.python.platform import resource_loader
from npu_bridge.helper import helper

gen_npu_cpu_ops = helper.get_gen_ops();


## 提供embeddingrankid功能
#  @param addr_tensor tensorflow的tensor类型，embeddingrankid操作的输入；
#  @param index tensorflow的tensor类型，embeddingrankid操作的输入；
#  @param row_memory int类型，一行数据存储的大小 默认为320。
#  @param mode string类型，embeddingrankid的操作类型，可以为”mod”,”order”;数据存储的方式。
#  @return 对输入addr_tensor，index_tensor执行完embeddingrankid操作之后的结果tensor
def embeddingrankid(addr_tensor, index, row_memory=320, mode='mod'):
    result = gen_npu_cpu_ops.embedding_rank_id(
        addr_table=addr_tensor,
        index=index,
        row_memory=row_memory,
        mode=mode)
    return result

## 提供embeddinglocalindex功能
#  @param addr_tensor tensorflow的tensor类型，embeddinglocalindex操作的输入；
#  @param index tensorflow的tensor类型，embeddinglocalindex操作的输入；
#  @param row_memory int类型，一行数据存储的大小 默认为320。
#  @param mode string类型，embeddinglocalindex的操作类型，可以为”mod”,”order”;数据存储的方式。
#  @return 对输入addr_tensor，index_tensor执行完embeddinglocalindex操作之后的结果tensor
def embedding_local_index(addr_tensor, index, row_memory=320, mode='mod'):
    result = gen_npu_cpu_ops.embedding_local_index(
        addr_table=addr_tensor,
        index=index,
        row_memory=row_memory,
        mode=mode)
    return result

## 提供RandomChoiceWithMask功能
#  @param x bool 类型
#  @param count int 类型
#  @param seed int类型
#  @param seed2 int类型
#  @return y int32类型 mask bool 类型
def randomchoicewithmask(x, count, seed=0, seed2=0):
    result = gen_npu_cpu_ops.random_choice_with_mask(
        x=x,
        count=count,
        seed=seed,
        seed2=seed2)
    return result

## 提供DenseImageWarp功能
#  @param image tensor类型
#  @param flow tensor类型
#  @return y tensor类型
def dense_image_warp(image, flow, name=None):
    result = gen_npu_cpu_ops.dense_image_warp(
        image=image,
        flow=flow,
        name=name
    )
    return result

## DenseImageWarp的梯度函数
@ops.RegisterGradient("DenseImageWarp")
def dense_image_warp_grad(op, grad):
    image = op.inputs[0]
    flow = op.inputs[1]
    grad_image, grad_flow = gen_npu_cpu_ops.dense_image_warp_grad(
        grad, image, flow)
    return [grad_image, grad_flow]

## 提供BatchEnqueue功能
#  @param x uint8 类型
#  @param queue_id uint32 类型
#  @param batch_size int 类型
#  @param queue_name string 类型
#  @param pad_mode string 类型
#  @return enqueue_count int64类型
def batch_enqueue(x, queue_id, batch_size=8, queue_name="", pad_mode="REPLICATE"):
    result = gen_npu_cpu_ops.batch_enqueue(
        x=x,
        queue_id=queue_id,
        batch_size=batch_size,
        queue_name=queue_name,
        pad_mode=pad_mode)
    return result

## 提供OCRRecognitionPreHandle功能
#  @param imgs_data uint8 类型
#  @param imgs_offset int32 类型
#  @param imgs_size int32 类型
#  @param langs int32 类型
#  @param langs_score int32 类型
#  @param batch_size int 类型
#  @param data_format string 类型
#  @param pad_mode string 类型
#  @return imgs,imgs_relation,imgs_lang uint8,int32,int32 类型
def ocr_recognition_pre_handle(imgs_data, imgs_offset, imgs_size, langs, langs_score, batch_size=8, data_format="NHWC", pad_mode="REPLICATE"):
    result = gen_npu_cpu_ops.ocr_recognition_pre_handle(
        imgs_data=imgs_data,
        imgs_offset=imgs_offset,
        imgs_size=imgs_size,
        langs=langs,
        langs_score=langs_score,
        batch_size=batch_size,
        data_format=data_format,
        pad_mode=pad_mode)
    return result

## 提供OCRDetectionPreHandle功能
#  @param img uint8 类型
#  @param data_format string 类型
#  @return resized_img,h_scale,w_scale uint8,float32,float32 类型
def ocr_detection_pre_handle(img, data_format="NHWC"):
    result = gen_npu_cpu_ops.ocr_recognition_pre_handle(
        img=img,
        data_format=data_format)
    return result

## 提供OCRIdentifyPreHandle功能
#  @param imgs_data uint8 类型
#  @param imgs_offset int32 类型
#  @param imgs_size int32 类型
#  @param size list(int) 类型
#  @param data_format string 类型
#  @return resized_imgs, uint8 类型
def ocr_identify_pre_handle(imgs_data, imgs_offset, imgs_size, size, data_format="NHWC"):
    result = gen_npu_cpu_ops.ocr_recognition_pre_handle(
        imgs_data=imgs_data,
        imgs_offset=imgs_offset,
        imgs_size=imgs_size,
        size=size,
        data_format=data_format)
    return result