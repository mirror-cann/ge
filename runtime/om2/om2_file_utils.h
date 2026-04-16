/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_RUNTIME_OM2_OM2_FILE_UTILS_H_
#define GE_RUNTIME_OM2_OM2_FILE_UTILS_H_

#include <cstdint>
#include <string>

#include "ge/ge_api_types.h"

namespace ge {
namespace om2 {
std::string RealPath(const char_t *path);
void SplitFilePath(const std::string &file_path, std::string &dir_path, std::string &file_name);
int32_t CreateDir(const std::string &directory_path);
Status GetAscendWorkPath(std::string &ascend_work_path);
}  // namespace om2
}  // namespace ge

#endif  // GE_RUNTIME_OM2_OM2_FILE_UTILS_H_
