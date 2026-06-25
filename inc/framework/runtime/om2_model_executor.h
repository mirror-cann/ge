/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */


#ifndef AIR_CXX_RUNTIME_V2_CORE_OM2_MODEL_EXECUTOR_H_
#define AIR_CXX_RUNTIME_V2_CORE_OM2_MODEL_EXECUTOR_H_
#include <memory>
#include <vector>
#include "common/ge_visibility.h"
#include "common/ge_common/ge_types.h"
#include "framework/common/om2_tensor_desc.h"

namespace gert {
class RtSession;

struct VISIBILITY_EXPORT Om2ModelLoadArg {
  int32_t device_id = -1;
  uint32_t model_id = 0U;
  void *work_ptr = nullptr;
  size_t work_size = 0U;
  void *weight_ptr = nullptr;
  size_t weight_size = 0U;
  gert::RtSession *rt_session = nullptr;
  std::vector<ge::FileConstantMem> file_constant_mems;
  bool need_clear_dfx_cache = false;
};

class VISIBILITY_EXPORT Om2ModelExecutor {
 public:
  Om2ModelExecutor();
  ~Om2ModelExecutor();

  Om2ModelExecutor(const Om2ModelExecutor &) = delete;
  Om2ModelExecutor(Om2ModelExecutor &&) = delete;
  Om2ModelExecutor &operator=(const Om2ModelExecutor &) = delete;
  Om2ModelExecutor &operator=(Om2ModelExecutor &&) = delete;

  ge::Status Load(ge::ModelData &model_data, const Om2ModelLoadArg &load_arg, const uint64_t session_id) const;
  ge::Status Run(std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) const;
  ge::Status RunAsync(void *const stream, std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) const;
  ge::Status GetModelDescInfo(const std::vector<ge::Om2TensorDesc> *&input_desc,
                              const std::vector<ge::Om2TensorDesc> *&output_desc,
                              bool new_model_desc = false) const;
  ge::Status GetModelAttrs(std::vector<std::string> &dynamic_output_shape) const;
  ge::Status GetDynamicBatchInfo(std::vector<std::vector<int64_t>> &dynamic_batch_info, int32_t &dynamic_type) const;
  ge::Status GetUserDesignateShapeOrder(std::vector<std::string> &user_designate_shape_order) const;
  ge::Status SetDynamicSize(const std::vector<uint64_t> &batch_num, int32_t dynamic_type);
  ge::Status GetCurrentShape(std::vector<int64_t> &batch_info, int32_t &dynamic_type) const;
  const std::vector<std::vector<int64_t>> &GetOriginInputDims() const;
  ge::Status GetOpAttr(std::map<std::string, std::map<std::string, std::string>> &op_attr_map) const;
  ge::Status GetOpDescInfo(uint32_t device_id, uint32_t stream_id, uint32_t task_id,
                           ge::OpDescInfo &op_desc_info) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};


VISIBILITY_EXPORT ge::Status LoadOm2DataFromFile(const std::string &model_path, ge::ModelData &model_data);
VISIBILITY_EXPORT ge::Status GetOm2MemAndWeightSize(const std::string &model_path, size_t &work_size,
                                                    size_t &internal_weight_size);
VISIBILITY_EXPORT ge::Status GetOm2MemAndWeightSize(const void *model_data, size_t model_size,
                                                    size_t &work_size, size_t &internal_weight_size);
VISIBILITY_EXPORT ge::Status GetOm2ModelMetadata(const std::string &model_path,
                                                 std::vector<ge::Om2TensorDesc> &input_desc,
                                                 std::vector<ge::Om2TensorDesc> &input_desc_v2,
                                                 std::vector<ge::Om2TensorDesc> &output_desc,
                                                 std::vector<ge::Om2TensorDesc> &output_desc_v2);
VISIBILITY_EXPORT ge::Status GetOm2ModelMetadata(const void *model_data, size_t model_size,
                                                 std::vector<ge::Om2TensorDesc> &input_desc,
                                                 std::vector<ge::Om2TensorDesc> &input_desc_v2,
                                                 std::vector<ge::Om2TensorDesc> &output_desc,
                                                 std::vector<ge::Om2TensorDesc> &output_desc_v2);
VISIBILITY_EXPORT std::unique_ptr<Om2ModelExecutor> LoadOm2ExecutorFromData(ge::ModelData &model_data,
                                                                            const Om2ModelLoadArg &load_arg,
                                                                            ge::graphStatus &error_code);
VISIBILITY_EXPORT ge::Status IsOm2Model(const void *data, size_t size, bool &is_support);
VISIBILITY_EXPORT ge::Status IsOm2Model(const char *file_path, bool &is_support);
}  // namespace gert

#endif  // AIR_CXX_RUNTIME_V2_CORE_OM2_MODEL_EXECUTOR_H_
