/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_FRAMEWORK_COMMON_HELPER_OM2_PACKAGE_HELPER_H
#define INC_FRAMEWORK_COMMON_HELPER_OM2_PACKAGE_HELPER_H

#include "framework/common/helper/model_save_helper.h"
#include "common/om2/codegen/om2_codegen_types.h"
#include <memory>

namespace ge {
class ZipArchiveWriter;

class GE_FUNC_VISIBILITY Om2PackageHelper : public ModelSaveHelper {
 public:
  Om2PackageHelper() noexcept = default;

  ~Om2PackageHelper() override = default;

  Status SaveToOmRootModel(const GeRootModelPtr &ge_root_model, const std::string &output_file, ModelBufferData &model,
                           const bool is_unknown_shape) override;

  Status SaveToOmModel(const GeModelPtr &ge_model, const std::string &output_file, ModelBufferData &model,
                       const GeRootModelPtr &ge_root_model = nullptr) override;

  void SetSaveMode(const bool val) override;

  static Status RelocateExternalWeights(const std::string &output_file_name, const ModelBufferData &model,
                                        ModelBufferData &relocated_model, bool &relocated);

  /// @brief 从 OM2 ZIP 模型内提取 visual JSON 内容。
  /// @param model_data  OM2 ZIP 数据内存地址。
  /// @param model_len   OM2 ZIP 数据长度。
  /// @param json_out    输出 visual JSON 内容。
  static Status ExtractVisualJson(const void *model_data, size_t model_len, std::string &json_out);

 private:
  static Status SaveConstants(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                              const size_t model_index, const std::vector<Om2ConstMeta> &const_metas,
                              const bool save_file_path);
  static Status SaveTbeKernels(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model);
  static Status SaveCustAICpuKernels(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model);
  static Status SaveModelInfo(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                              const size_t model_index);
  static Status SaveOpAttrJson(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                               const size_t model_index);
  static Status SaveVisualJson(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                               const size_t model_index);
  static Status SaveGraphDebugFiles(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                    const size_t model_index);
  static Status SaveManifest(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeRootModelPtr &ge_root_model);
  static Status SaveCodegenArtifacts(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                     const size_t model_index, std::vector<Om2ConstMeta> &const_metas);

 private:
  bool is_offline_{true};
};
}  // namespace ge
#endif  // INC_FRAMEWORK_COMMON_HELPER_OM2_PACKAGE_HELPER_H
