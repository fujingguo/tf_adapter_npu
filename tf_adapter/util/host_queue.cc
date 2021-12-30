#include "tf_adapter/util/host_queue.h"

#include <securec.h>
#include "mmpa/mmpa_api.h"
#include "runtime/dev.h"
#include "runtime/rt_mem_queue.h"
#include "tf_adapter/common/adp_logger.h"
#include "tf_adapter/util/acl_channel.h"
#include "tf_adapter/util/npu_attrs.h"
#include "tf_adapter/util/ge_plugin.h"
#include "tf_adapter_2.x/npu_device/core/npu_micros.h"

namespace tensorflow {
namespace {
const static uint32_t kMaxValue = 128U;
const static uint32_t kMaxQueueDepth = 0x4fffffffU;

Status CheckSymbols() {
  if (rtMemQueueCreate == nullptr) { return errors::Internal("rtMemQueueCreate not found"); }
  if (rtMemQueueDestroy == nullptr) { return errors::Internal("rtMemQueueDestroy not found"); }
  if (rtMemQueueInit == nullptr) { return errors::Internal("rtMemQueueInit not found"); }
  if (rtMemQueueEnQueue == nullptr) { return errors::Internal("rtMemQueueEnQueue not found"); }
  if (rtMemQueueDeQueue == nullptr) { return errors::Internal("rtMemQueueDeQueue not found"); }
  if (rtMbufInit == nullptr) { return errors::Internal("rtMbufInit not found"); }
  if (rtMbufAlloc == nullptr) { return errors::Internal("rtMbufAlloc not found"); }
  if (rtMbufFree == nullptr) { return errors::Internal("rtMbufFree not found"); }
  if (rtMbufGetBuffAddr == nullptr) { return errors::Internal("rtMbufGetBuffAddr not found"); }
  if (rtMbufGetBuffSize == nullptr) { return errors::Internal("rtMbufGetBuffSize not found"); }
  return Status::OK();
}

Status GetDataTypeByTensorType(acltdtTensorType tensor_type, int32_t &data_type) {
  const static std::unordered_map<acltdtTensorType, int32_t> type_map = {
    {ACL_TENSOR_DATA_TENSOR, 0}, {ACL_TENSOR_DATA_END_OF_SEQUENCE, 1}, {ACL_TENSOR_DATA_ABNORMAL, 2}};
  auto ret = type_map.find(tensor_type);
  if (ret == type_map.end()) {
    ADP_LOG(ERROR) << "invalid tensor_type: " << tensor_type;
    return errors::Internal("invalid tensor type : ", tensor_type);
  }

  data_type = ret->second;
  ADP_LOG(INFO) << "get data type[" << data_type << "] by tensor type[" << tensor_type << "] success";
  return Status::OK();
}

Status AddDataItemInfo(acltdtTensorType tdt_data_type, int32_t tensor_type, const int64_t *dims, size_t dim_size,
                       void *data_ptr, uint64_t data_len, std::vector<DataItemInfo> &items) {
  DataItemInfo item = {};
  int32_t data_type = 0;
  TF_RETURN_IF_ERROR(GetDataTypeByTensorType(tdt_data_type, data_type));
  item.ctrl_info.data_type = data_type;
  item.ctrl_info.tensor_type = tensor_type;
  item.ctrl_info.dim_num = dim_size;
  item.ctrl_info.data_len = data_len;
  item.dims.clear();
  for (size_t i = 0UL; i < dim_size; ++i) {
    item.dims.push_back(dims[i]);
  }
  item.data_ptr = data_ptr;
  items.push_back(item);
  return Status::OK();
}

Status MappingTensors2DataItemInfos(acltdtTensorType acl_type, const std::vector<Tensor> &tensors,
                                    std::vector<DataItemInfo> &items) {
  if (acl_type != ACL_TENSOR_DATA_TENSOR) {
    return AddDataItemInfo(acl_type, ACL_BOOL, nullptr, 0UL, nullptr, 0UL, items);
  }

  for (auto &tensor : tensors) {
    aclDataType acl_data_type;
    TF_RETURN_IF_ERROR(MappingTfDtypeToAcl(tensor.dtype(), acl_data_type));
    auto dims = tensor.shape().dim_sizes();
    if (DataTypeCanUseMemcpy(tensor.dtype())) {
      TF_RETURN_IF_ERROR(AddDataItemInfo(ACL_TENSOR_DATA_TENSOR, acl_data_type,
                                         (dims.empty() ? nullptr : reinterpret_cast<const int64_t *>(dims.data())),
                                         dims.size(), const_cast<char *>(tensor.tensor_data().data()),
                                         tensor.tensor_data().size(), items));
    } else if (tensor.dtype() == DT_STRING) {
      if (tensor.dims() != 0) {
        return errors::Internal("got unexpected non-scalar string tensor with dim ", tensor.dims());
      }
      auto value = reinterpret_cast<tensorflow::tstring *>(const_cast<char *>(tensor.tensor_data().data()));
      TF_RETURN_IF_ERROR(AddDataItemInfo(ACL_TENSOR_DATA_TENSOR, ACL_STRING, nullptr, 0UL,
                                         const_cast<char *>(value->c_str()), value->size(), items));
    } else {
      return errors::Internal("unexpected data type ", DataTypeString(tensor.dtype()));
    }
  }
  return Status::OK();
}

Status SerializeDataItemInfo(std::vector<DataItemInfo> &items, void *&buff) {
  size_t cnt = items.size();
  size_t total_size = 0UL;
  for (size_t i = 0UL; i < cnt; ++i) {
    items[i].ctrl_info.cur_cnt = i;
    items[i].ctrl_info.cnt = cnt;
    total_size += sizeof(ItemInfo) + items[i].ctrl_info.dim_num * sizeof(int64_t) + items[i].ctrl_info.data_len;
  }

  auto rt_error = rtMbufAlloc(&buff, total_size);
  if (rt_error != ACL_RT_SUCCESS) {
    return errors::Internal("call rtMbufAlloc with size[", total_size, "] failed, ret = ", rt_error);
  }

  void *data = nullptr;
  rt_error = rtMbufGetBuffAddr(buff, &data);
  if (rt_error != ACL_RT_SUCCESS) {
    (void)rtMbufFree(buff);
    return errors::Internal("call rtMbufAlloc with size[", total_size, "] failed, ret = ", rt_error);
  }

  size_t offset = 0UL;
  for (size_t i = 0UL; i < cnt; ++i) {
    auto ret = memcpy_s(data + offset, total_size - offset, &items[i].ctrl_info, sizeof(ItemInfo));
    if (ret != EOK) {
      (void)rtMbufFree(buff);
      return errors::Internal("call memcpy_s failed, ret = ", ret, ", error = ", strerror(errno));
    }
    offset += sizeof(ItemInfo);

    for (size_t j = 0UL; j < items[i].ctrl_info.dim_num; ++j) {
      ret = memcpy_s(data + offset, total_size - offset, &(items[i].dims[j]), sizeof(int64_t));
      if (ret != EOK) {
        (void)rtMbufFree(buff);
        return errors::Internal("call memcpy_s failed, ret = ", ret, ", error = ", strerror(errno));
      }
      offset += sizeof(int64_t);
    }

    if (items[i].ctrl_info.data_len == 0UL) { continue; }

    ret = memcpy_s(data + offset, total_size - offset, items[i].data_ptr, items[i].ctrl_info.data_len);
    if (ret != EOK) {
      (void)rtMbufFree(buff);
      return errors::Internal("call memcpy_s failed, ret = ", ret, ", error = ", strerror(errno));
    }
    offset += items[i].ctrl_info.data_len;
  }

  return Status::OK();
}
}  // namespace

Status HostQueueInit(const std::string &name, const uint32_t &depth, uint32_t &queue_id) {
  TF_RETURN_IF_ERROR(CheckSymbols());
  ADP_LOG(INFO) << "init ge";
  std::map<std::string, std::string> init_options = NpuAttrs::GetInitOptions();
  NpuAttrs::LogOptions(init_options);
  GePlugin::GetInstance()->Init(init_options, false);

  NPU_REQUIRES(name.size() + 1 <= RT_MQ_MAX_NAME_LEN,
               errors::Internal("invalid queue name length[", name.size(), "], should less than 128"));

  NPU_REQUIRES(depth < kMaxQueueDepth,
               errors::Internal("invalid queue depth[", depth, "], should less than ", kMaxQueueDepth));

  auto rt_error = rtSetDevice(0);
  NPU_REQUIRES(rt_error == ACL_RT_SUCCESS, errors::Internal("call rtSetDevice device[0] failed, ret=", rt_error));
  ADP_LOG(INFO) << "call rtSetDevice device[0] success";

  rt_error = rtMemQueueInit(0);
  NPU_REQUIRES(((rt_error == ACL_RT_SUCCESS) || (rt_error == ACL_ERROR_RT_REPEATED_INIT)),
               errors::Internal("call rtMemQueueInit device[0] failed, ret=", rt_error));

  ADP_LOG(INFO) << "call rtMemQueueInit with device[0] success";

  rtMemQueueAttr_t attr = {};
  (void)memset_s(attr.name, RT_MQ_MAX_NAME_LEN, 0, RT_MQ_MAX_NAME_LEN);
  auto ret = memcpy_s(attr.name, RT_MQ_MAX_NAME_LEN, name.c_str(), name.size() + 1);
  NPU_REQUIRES(ret == EOK, errors::Internal("call memcpy_s queue name failed, ret=", ret));

  attr.depth = depth;
  attr.workMode = RT_MQ_MODE_DEFAULT;
  attr.flowCtrlFlag = false;
  attr.flowCtrlDropTime = 0U;
  attr.overWriteFlag = false;
  rt_error = rtMemQueueCreate(0, &attr, &queue_id);
  NPU_REQUIRES(rt_error == ACL_RT_SUCCESS, errors::Internal("call rtMemQueueCreate device[0] failed, ret=", rt_error));

  ADP_LOG(INFO) << "call rtMemQueueCreate with device[0] queue[" << queue_id << "] success";

  rtMemBuffCfg_t buff_cfg = {0};
  rt_error = rtMbufInit(&buff_cfg);
  NPU_REQUIRES(((rt_error == ACL_RT_SUCCESS) || (rt_error == ACL_ERROR_RT_REPEATED_INIT)),
               errors::Internal("call rtMbufInit failed, ret=", ret));

  return Status::OK();
}

void HostQueueDestroy(const uint32_t &queue_id) {
  ADP_LOG(INFO) << "begin host queue: " << queue_id << " destroy";
  auto rt_error = rtSetDevice(0);
  if (rt_error != ACL_RT_SUCCESS) {
    ADP_LOG(ERROR) << "call rtSetDevice device[0] failed, ret=" << rt_error;
  }

  rt_error = rtMemQueueDestroy(0, queue_id);
  if (rt_error != ACL_RT_SUCCESS) {
    ADP_LOG(ERROR) << "call rtMemQueueDestroy device[0] queue[" << queue_id << "] failed, ret=" << rt_error;
  }
}

Status MappingTensor2Buff(const acltdtTensorType &acl_type, const std::vector<tensorflow::Tensor> &tensors,
                          void *&buff) {
  std::vector<DataItemInfo> items;
  TF_RETURN_IF_ERROR(MappingTensors2DataItemInfos(acl_type, tensors, items));
  TF_RETURN_IF_ERROR(SerializeDataItemInfo(items, buff));
  return Status::OK();
}

Status HostQueueSendData(uint32_t queue_id, void *buff, bool &need_resend) {
  need_resend = false;
  auto rt_error = rtSetDevice(0);
  NPU_REQUIRES(rt_error == ACL_RT_SUCCESS, errors::Internal("call rtSetDevice device[0] failed, ret=", rt_error));
  rt_error = rtMemQueueEnQueue(0, queue_id, buff);
  if (rt_error == RT_ERROR_NONE) {
    return Status::OK();
  } else if (rt_error == ACL_ERROR_RT_QUEUE_FULL) {
    need_resend = true;
    ADP_LOG(INFO) << "queue[" << queue_id << "] is full, need call rtMemQueueEnQueue again";
  } else {
    HostQueueFreeBuff(buff);
    return errors::Internal("host enqueue queue[", queue_id, "] failed, ret = ", rt_error);
  }

  return Status::OK();
}

void HostQueueFreeBuff(void *buff) {
  auto rt_error = rtMbufFree(buff);
  if (rt_error != ACL_RT_SUCCESS) {
    ADP_LOG(ERROR) << "call rtMbufFree failed, ret=" << rt_error;
  }
}
}  // namespace tensorflow