/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_code_printer.h"

namespace ge {

void Om2CodePrinter::AddContent(GeneratedFileIndex generated_file_index, const std::string &input_string) {
  output_[static_cast<size_t>(generated_file_index)].content << input_string;
}

void Om2CodePrinter::SetFileInfo(GeneratedFileIndex generated_file_index, const std::string &file_name) {
  output_[static_cast<size_t>(generated_file_index)].file_name = file_name;
}

void Om2CodePrinter::InitDefaultFileInfo(const std::string &model_name) {
  SetFileInfo(GeneratedFileIndex::kInterfaceHeaderFile, model_name + "_interface.h");
  SetFileInfo(GeneratedFileIndex::kResourcesFile, model_name + "_resources.cpp");
  SetFileInfo(GeneratedFileIndex::kArgsManagerFile, model_name + "_args_manager.cpp");
  SetFileInfo(GeneratedFileIndex::kKernelRegistryFile, model_name + "_kernel_reg.cpp");
  SetFileInfo(GeneratedFileIndex::kLoadingAndRunningFile, model_name + "_load_and_run.cpp");
  SetFileInfo(GeneratedFileIndex::kCMakeListsFile, "Makefile");
}

void Om2CodePrinter::GetOutputFiles(Om2CodegenArtifacts &artifacts) const {
  artifacts.clear();
  for (const auto &generated_file_info : output_) {
    if (generated_file_info.file_name.empty()) {
      continue;
    }
    artifacts.push_back({generated_file_info.file_name, generated_file_info.content.str()});
  }
}
}  // namespace ge
