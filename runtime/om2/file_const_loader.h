/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_RUNTIME_OM2_FILE_CONST_LOADER_H_
#define AIR_RUNTIME_OM2_FILE_CONST_LOADER_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "common/ge_common/ge_types.h"

namespace gert {
struct Om2ConstItem {
  size_t index = 0U;
  std::string type;
  std::string file_name;
  size_t offset = 0U;
  size_t size = 0U;
};

struct FileConstContext {
  std::string weight_dir;
  const std::map<std::string, ge::FileConstantMem> *user_file_const_mems = nullptr;
  std::vector<void *> *owned_buffers = nullptr;
  uint64_t session_id = 0U;
  int32_t device_id = -1;
};

ge::Status BuildUserFileConstMemMap(const std::vector<ge::FileConstantMem> &file_constant_mems,
                                    std::map<std::string, ge::FileConstantMem> &file_name_to_mem);
ge::Status ResolveFileConstWeightDir(const ge::ModelData &model_data, std::string &weight_dir);
ge::Status ResolveFileConstFilePath(const std::string &weight_dir, const std::string &file_name,
                                    std::string &file_path);
ge::Status PrepareCombinedConsts(const std::vector<Om2ConstItem> &const_items, const FileConstContext &ctx,
                                 std::vector<void *> &constants);
ge::Status PrepareIndividualConst(const Om2ConstItem &const_item, const FileConstContext &ctx, void *&const_addr);
ge::Status PrepareIndividualConsts(const std::vector<Om2ConstItem> &const_items, const FileConstContext &ctx,
                                   int32_t device_id, std::vector<void *> &constants);
}  // namespace gert

#endif  // AIR_RUNTIME_OM2_FILE_CONST_LOADER_H_
