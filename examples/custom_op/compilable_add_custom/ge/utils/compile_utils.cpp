/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "graph/custom_op.h"

namespace {
constexpr const char *kPlatformInfoVersionGroup = "version";
constexpr const char *kPlatformInfoNpuArch = "NpuArch";
constexpr const char *kRtcNpuArchPrefix = "dav-";
constexpr const char *kKernelSourceFileName = "add_custom_kernel.cpp";

bool GetNpuArch(fe::PlatFormInfos &platform_infos, std::string &npu_arch) {
  if (!platform_infos.GetPlatformResWithLock(kPlatformInfoVersionGroup, kPlatformInfoNpuArch, npu_arch)) {
    npu_arch.clear();
    return false;
  }
  return !npu_arch.empty();
}

std::string NormalizeRtcNpuArch(const std::string &npu_arch) {
  if (npu_arch.rfind(kRtcNpuArchPrefix, 0U) == 0U) {
    return npu_arch;
  }
  return std::string(kRtcNpuArchPrefix) + npu_arch;
}

std::string GetCurrentLibraryDir() {
  Dl_info info{};
  if ((dladdr(reinterpret_cast<void *>(&GetCurrentLibraryDir), &info) == 0) || (info.dli_fname == nullptr)) {
    return {};
  }
  const std::string library_path = info.dli_fname;
  const auto pos = library_path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0) {
    return "/";
  }
  return library_path.substr(0, pos);
}
}  // namespace

namespace compile_utils {
std::string BuildBinaryKey(const gert::Shape &shape);

ge::graphStatus BuildRtcCompileOptionFromContext(gert::OpCompileContext *ctx, std::string &rtc_compile_option) {
  if (ctx == nullptr) {
    std::cerr << __FILE__ << ":" << __LINE__ << " Compile context is null, can not build rtc compile option"
              << std::endl;
    return ge::GRAPH_FAILED;
  }
  fe::PlatFormInfos platform_infos;
  fe::OptionalInfos optional_infos;
  const auto ret = ctx->GetPlatformInfos(platform_infos, optional_infos);
  if (ret != ge::GRAPH_SUCCESS) {
    std::cerr << __FILE__ << ":" << __LINE__ << " GetPlatformInfos failed, ret: " << ret << std::endl;
    return ret;
  }
  std::string npu_arch;
  if (!GetNpuArch(platform_infos, npu_arch)) {
    std::cerr << __FILE__ << ":" << __LINE__
              << " failed to get NpuArch from platform infos, optional soc_version: " << optional_infos.GetSocVersion()
              << std::endl;
    return ge::GRAPH_FAILED;
  }
  rtc_compile_option = std::string("--npu-arch=") + NormalizeRtcNpuArch(npu_arch);
  std::cout << __FILE__ << ":" << __LINE__ << " build rtc compile option from " << kPlatformInfoNpuArch
            << ", optional soc_version: " << optional_infos.GetSocVersion() << ", option: " << rtc_compile_option
            << std::endl;
  return ge::GRAPH_SUCCESS;
}

void DumpCompileInputTensor(gert::OpCompileContext *ctx, size_t index) {
  if (ctx == nullptr) {
    std::cout << __FILE__ << ":" << __LINE__ << " compile context is null" << std::endl;
    return;
  }
  const auto *const input_tensor = ctx->GetInputTensor(index);
  if (input_tensor == nullptr) {
    std::cout << __FILE__ << ":" << __LINE__ << " compile input tensor[" << index << "] is null" << std::endl;
    return;
  }

  const auto &storage_shape = input_tensor->GetShape().GetStorageShape();
  const auto &origin_shape = input_tensor->GetShape().GetOriginShape();
  std::cout << __FILE__ << ":" << __LINE__ << " compile input tensor[" << index << "]"
            << ", storage_shape: " << BuildBinaryKey(storage_shape)
            << ", origin_shape: " << BuildBinaryKey(origin_shape) << ", data_type: " << input_tensor->GetDataType()
            << ", origin_format: " << input_tensor->GetOriginFormat()
            << ", storage_format: " << input_tensor->GetStorageFormat() << std::endl;
}

std::string BuildBinaryKey(const gert::Shape &shape) {
  std::ostringstream builder;
  builder << "[";
  for (size_t i = 0U; i < shape.GetDimNum(); ++i) {
    if (i != 0U) {
      builder << ",";
    }
    builder << shape.GetDim(i);
  }
  builder << "]";
  return builder.str();
}

void DumpComputeNodeInfo(gert::OpCompileContext *ctx) {
  if (ctx == nullptr) {
    std::cout << __FILE__ << ":" << __LINE__ << " skip GetComputeNodeInfo because ctx is null" << std::endl;
    return;
  }
  const auto *const compute_node_info = ctx->GetComputeNodeInfo();
  if (compute_node_info == nullptr) {
    std::cout << __FILE__ << ":" << __LINE__ << " compute_node_info is null" << std::endl;
    return;
  }

  const auto inputs_num = compute_node_info->GetInputsNum();
  const auto outputs_num = compute_node_info->GetOutputsNum();
  std::cout << __FILE__ << ":" << __LINE__ << " compute_node_info node_name: "
            << ((compute_node_info->GetNodeName() == nullptr) ? "<null>" : compute_node_info->GetNodeName())
            << ", node_type: "
            << ((compute_node_info->GetNodeType() == nullptr) ? "<null>" : compute_node_info->GetNodeType())
            << ", ir_inputs_num: " << compute_node_info->GetIrInputsNum() << ", inputs_num: " << inputs_num
            << ", outputs_num: " << outputs_num << std::endl;
}

void DumpCompileOption(gert::OpCompileContext *ctx, const char *option_key) {
  if ((ctx == nullptr) || (option_key == nullptr)) {
    std::cout << __FILE__ << ":" << __LINE__ << " skip GetOption because ctx or option_key is null" << std::endl;
    return;
  }
  ge::AscendString option_key_str(option_key);
  ge::AscendString option_value;
  const auto ret = ctx->GetOption(option_key_str, option_value);
  if (ret != ge::GRAPH_SUCCESS) {
    std::cout << __FILE__ << ":" << __LINE__ << " ctx->GetOption(" << option_key << ") ret: " << ret << std::endl;
    return;
  }
  const auto *const value_str = option_value.GetString();
  std::cout << __FILE__ << ":" << __LINE__ << " ctx->GetOption(" << option_key << ") ret: " << ret
            << ", value: " << ((value_str == nullptr) ? "<null>" : value_str) << std::endl;
}

void DumpPlatformInfos(gert::OpCompileContext *ctx) {
  if (ctx == nullptr) {
    std::cout << __FILE__ << ":" << __LINE__ << " skip GetPlatformInfos because ctx is null" << std::endl;
    return;
  }
  fe::PlatFormInfos platform_infos;
  fe::OptionalInfos optional_infos;
  const auto ret = ctx->GetPlatformInfos(platform_infos, optional_infos);
  std::cout << __FILE__ << ":" << __LINE__ << " ctx->GetPlatformInfos ret: " << ret << std::endl;
  if (ret != ge::GRAPH_SUCCESS) {
    return;
  }

  const auto core_num = platform_infos.GetCoreNum();
  std::string npu_arch;
  const bool got_npu_arch = GetNpuArch(platform_infos, npu_arch);
  std::cout << __FILE__ << ":" << __LINE__ << " platform_infos core_num: " << core_num
            << ", got_npu_arch: " << got_npu_arch << ", npu_arch_key: " << kPlatformInfoNpuArch
            << ", npu_arch: " << (got_npu_arch ? npu_arch : "<unavailable>") << std::endl;

  std::cout << __FILE__ << ":" << __LINE__ << " optional_infos soc_version: " << optional_infos.GetSocVersion()
            << ", ai_core_num: " << optional_infos.GetAICoreNum() << std::endl;
}

std::string GetKernelSourcePath() {
  const auto library_dir = GetCurrentLibraryDir();
  if (library_dir.empty()) {
    return {};
  }
  if (library_dir == "/") {
    return library_dir + kKernelSourceFileName;
  }
  return library_dir + "/" + kKernelSourceFileName;
}

std::string LoadTextFromFile(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::in);
  if (!file) {
    return {};
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}
}  // namespace compile_utils
