/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_file_utils.h"

#include <cerrno>
#include <cstring>
#include <vector>

#include "base/err_msg.h"
#include "common/checker.h"
#include "common/ge_common/debug/ge_log.h"
#include "mmpa/mmpa_api.h"

namespace {
constexpr size_t kMaxErrorStrLen = 128U;

int32_t CheckAndMkdir(const ge::char_t *tmp_dir_path, mmMode_t mode) {
  if (mmAccess2(tmp_dir_path, M_F_OK) != EN_OK) {
    const int32_t ret = mmMkdir(tmp_dir_path, mode);
    if (ret != 0) {
      std::vector<ge::char_t> err_buf(kMaxErrorStrLen + 1U, '\0');
      const auto err_msg = mmGetErrorFormatMessage(mmGetErrorCode(), err_buf.data(), kMaxErrorStrLen);
      const std::string reason =
          "Directory creation failed. [Errno " + std::to_string(mmGetErrorCode()) + "] " + err_msg + ".";
      (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const ge::char_t *>({"parameter", "value", "reason"}),
                                      std::vector<const ge::char_t *>({"filepath", tmp_dir_path, reason.c_str()}));
      GELOGW("[OM2][Util][Mkdir] Create directory %s failed, reason:%s. Make sure the directory exists and writable.",
             tmp_dir_path, strerror(errno));
      return ret;
    }
  }
  return 0;
}
}  // namespace

namespace ge {
namespace om2 {
std::string RealPath(const char_t *path) {
  if (path == nullptr) {
    REPORT_INNER_ERR_MSG("E18888", "path is nullptr, check invalid");
    GELOGE(FAILED, "[OM2][Check][Param] path pointer is NULL.");
    return "";
  }
  GE_ASSERT_TRUE((strnlen(path, static_cast<size_t>(MMPA_MAX_PATH)) < static_cast<size_t>(MMPA_MAX_PATH)),
                 "[OM2][Check][Param] Path[%s] len is too long, it must be less than %d", path, MMPA_MAX_PATH);

  std::string res;
  char_t resolved_path[MMPA_MAX_PATH] = {};
  if (mmRealPath(path, &(resolved_path[0U]), MMPA_MAX_PATH) == EN_OK) {
    res = &(resolved_path[0]);
  } else {
    GELOGW("[OM2][Util][RealPath] Cannot get real_path for [%s], reason:%s", path, strerror(errno));
  }
  return res;
}

void SplitFilePath(const std::string &file_path, std::string &dir_path, std::string &file_name) {
  if (file_path.empty()) {
    GELOGD("[OM2] file_path is empty, no need split");
    return;
  }
  int32_t split_pos = static_cast<int32_t>(file_path.length() - 1UL);
  for (; split_pos >= 0; split_pos--) {
    if ((file_path[static_cast<size_t>(split_pos)] == '\\') || (file_path[static_cast<size_t>(split_pos)] == '/')) {
      break;
    }
  }
  if (split_pos < 0) {
    file_name = file_path;
    return;
  }
  dir_path = file_path.substr(0U, static_cast<size_t>(split_pos));
  file_name = file_path.substr(static_cast<size_t>(split_pos) + 1UL, file_path.length());
}

int32_t CreateDir(const std::string &directory_path) {
  GE_CHK_BOOL_EXEC(!directory_path.empty(), REPORT_INNER_ERR_MSG("E18888", "directory path is empty, check invalid");
                   return -1, "[OM2][Check][Param] directory path is empty.");
  const auto dir_path_len = directory_path.length();
  GE_CHK_BOOL_EXEC(dir_path_len < static_cast<size_t>(MMPA_MAX_PATH), return -1,
                   "[OM2][Util][Mkdir] Path %s len is too long, it must be less than %d", directory_path.c_str(),
                   MMPA_MAX_PATH);

  std::string current_path;
  current_path.reserve(dir_path_len);
  constexpr uint32_t mkdir_mode =
      static_cast<uint32_t>(M_IRUSR) | static_cast<uint32_t>(M_IWUSR) | static_cast<uint32_t>(M_IXUSR);
  const auto mode = static_cast<mmMode_t>(mkdir_mode);
  for (const char c : directory_path) {
    current_path += c;
    if (c == '\\' || c == '/') {
      const int32_t ret = CheckAndMkdir(current_path.c_str(), mode);
      if (ret != 0) {
        return ret;
      }
    }
  }
  return CheckAndMkdir(directory_path.c_str(), mode);
}

Status GetAscendWorkPath(std::string &ascend_work_path) {
  const char_t *work_path = nullptr;
  MM_SYS_GET_ENV(MM_ENV_ASCEND_WORK_PATH, work_path);
  if (work_path != nullptr) {
    if (mmAccess(work_path) != EN_OK) {
      if (CreateDir(work_path) != 0) {
        const std::string reason = "The path doesn't exist, create path failed.";
        (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char_t *>({"parameter", "value", "reason"}),
                                        std::vector<const char_t *>({"ASCEND_WORK_PATH", work_path, reason.c_str()}));
        return FAILED;
      }
    }
    ascend_work_path = RealPath(work_path);
    if (ascend_work_path.empty()) {
      GELOGE(FAILED, "[OM2][Call][RealPath] File path %s is invalid.", work_path);
      return FAILED;
    }
    GELOGD("[OM2] Get ASCEND_WORK_PATH success, path = %s, real path = %s", work_path, ascend_work_path.c_str());
    return SUCCESS;
  }
  ascend_work_path = "";
  GELOGD("[OM2] Get ASCEND_WORK_PATH fail");
  return SUCCESS;
}
}  // namespace om2
}  // namespace ge
