/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_RUNNING_ENV_SCOPED_UNSET_LD_PRELOAD_H_
#define GE_RUNNING_ENV_SCOPED_UNSET_LD_PRELOAD_H_

#include <cstdlib>
#include <string>

class ScopedUnsetLdPreload {
 public:
  ScopedUnsetLdPreload() {
    const char *ld_preload = std::getenv("LD_PRELOAD");
    if (ld_preload != nullptr) {
      has_ld_preload_ = true;
      ld_preload_ = ld_preload;
      (void)unsetenv("LD_PRELOAD");
    }
  }

  ~ScopedUnsetLdPreload() {
    if (has_ld_preload_) {
      (void)setenv("LD_PRELOAD", ld_preload_.c_str(), 1);
    }
  }

 private:
  bool has_ld_preload_{false};
  std::string ld_preload_;
};

#endif  // GE_RUNNING_ENV_SCOPED_UNSET_LD_PRELOAD_H_
