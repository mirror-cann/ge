/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stable_part_provider.h"

namespace ge {
Status ResolveStablePart(StablePartId id, std::string &output) {
  switch (id) {
    case StablePartId::kChkStatusMacro:
      output =
          "#define OM2_CHK_STATUS(expr, ...)            \\\n"
          "do {                                       \\\n"
          "  const aclError _chk_status = (expr);     \\\n"
          "  if (_chk_status != ACL_SUCCESS) {        \\\n"
          "    return _chk_status;                    \\\n"
          "  }                                        \\\n"
          "} while (false)\n";
      return SUCCESS;
    case StablePartId::kChkNotNullMacro:
      output =
          "#define OM2_CHK_NOTNULL(ptr, ...)            \\\n"
          "do {                                       \\\n"
          "  if ((ptr) == nullptr) {                  \\\n"
          "    return ACL_ERROR_FAILURE;              \\\n"
          "  }                                        \\\n"
          "} while (false)\n";
      return SUCCESS;
    case StablePartId::kChkTrueMacro:
      output =
          "#define OM2_CHK_TRUE(expr, ...)              \\\n"
          "do {                                       \\\n"
          "  if (!(expr)) {                           \\\n"
          "    return ACL_ERROR_FAILURE;              \\\n"
          "  }                                        \\\n"
          "} while (false)\n";
      return SUCCESS;
    case StablePartId::kGetAddrMacro:
      output =
          "#define GET_ADDR(mem_ptr, offset)   \\\n"
          "(reinterpret_cast<void *>(                 \\\n"
          "  reinterpret_cast<uintptr_t>(mem_ptr) +   \\\n"
          "  static_cast<uintptr_t>(offset)))\n";
      return SUCCESS;
    case StablePartId::kMakeGuardMacro:
      output = "#define OM2_MAKE_GUARD(var, callback) const ::om2::ScopeGuard const_guard_##var(callback)\n";
      return SUCCESS;
    case StablePartId::kInterfaceMacros:
      output =
          "#define OM2_CHK_STATUS(expr, ...)            \\\n"
          "do {                                       \\\n"
          "  const aclError _chk_status = (expr);     \\\n"
          "  if (_chk_status != ACL_SUCCESS) {        \\\n"
          "    return _chk_status;                    \\\n"
          "  }                                        \\\n"
          "} while (false)\n"
          "\n"
          "#define OM2_CHK_NOTNULL(ptr, ...)            \\\n"
          "do {                                       \\\n"
          "  if ((ptr) == nullptr) {                  \\\n"
          "    return ACL_ERROR_FAILURE;              \\\n"
          "  }                                        \\\n"
          "} while (false)\n"
          "\n"
          "#define OM2_CHK_TRUE(expr, ...)              \\\n"
          "do {                                       \\\n"
          "  if (!(expr)) {                           \\\n"
          "    return ACL_ERROR_FAILURE;              \\\n"
          "  }                                        \\\n"
          "} while (false)\n"
          "\n"
          "#define OM2_CHK_RT(expr, ...)               \\\n"
          "do {                                       \\\n"
          "  const rtError_t _rt_err = (expr);        \\\n"
          "  if (_rt_err != RT_ERROR_NONE) {          \\\n"
          "    return ACL_ERROR_FAILURE;              \\\n"
          "  }                                        \\\n"
          "} while (false)\n"
          "\n"
          "#define GET_ADDR(mem_ptr, offset)   \\\n"
          "(reinterpret_cast<void *>(                 \\\n"
          "  reinterpret_cast<uintptr_t>(mem_ptr) +   \\\n"
          "  static_cast<uintptr_t>(offset)))\n"
          "\n"
          "#define OM2_MAKE_GUARD(var, callback) const ::om2::ScopeGuard const_guard_##var(callback)\n";
      return SUCCESS;
    case StablePartId::kPointerHelpers:
      output =
          "template<typename T>\n"
          "inline T *PtrAdd(T *const ptr, const size_t max_buf_len, const size_t idx) {\n"
          "  if ((ptr != nullptr) && (idx < max_buf_len)) {\n"
          "    return reinterpret_cast<T *>(ptr + idx);\n"
          "  }\n"
          "  return nullptr;\n"
          "}\n"
          "template<typename TI, typename TO>\n"
          "inline TO *PtrToPtr(TI *const ptr) {\n"
          "  return reinterpret_cast<TO *>(ptr);\n"
          "}\n"
          "inline uint64_t PtrToValue(const void *const ptr) {\n"
          "  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));\n"
          "}\n"
          "inline void *ValueToPtr(const uint64_t value) {\n"
          "  return reinterpret_cast<void *>(static_cast<uintptr_t>(value));\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kInterfacePointerHelpers:
      output =
          "template<typename T>\n"
          "inline T *PtrAdd(T *const ptr, const size_t max_buf_len, const size_t idx) {\n"
          "  if ((ptr != nullptr) && (idx < max_buf_len)) {\n"
          "    return reinterpret_cast<T *>(ptr + idx);\n"
          "  }\n"
          "  return nullptr;\n"
          "}\n"
          "template<typename TI, typename TO>\n"
          "inline TO *PtrToPtr(TI *const ptr) {\n"
          "  return reinterpret_cast<TO *>(ptr);\n"
          "}\n"
          "inline uint64_t PtrToValue(const void *const ptr) {\n"
          "  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));\n"
          "}\n"
          "inline void *ValueToPtr(const uint64_t value) {\n"
          "  return reinterpret_cast<void *>(static_cast<uintptr_t>(value));\n"
          "}\n"
          "\n"
          "template<typename... Args>\n"
          "inline std::vector<uint64_t> FlattenHostArgs(Args&&... args) {\n"
          "  std::vector<uint64_t> buf;\n"
          "  auto append_arg = [&](auto&& arg) {\n"
          "    using T = std::decay_t<decltype(arg)>;\n"
          "    if constexpr (std::is_pointer_v<T>) {\n"
          "      buf.push_back(reinterpret_cast<uint64_t>(arg));\n"
          "    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {\n"
          "      for (auto d : arg) buf.push_back(static_cast<uint64_t>(d));\n"
          "    } else if constexpr (std::is_integral_v<T>) {\n"
          "      buf.push_back(static_cast<uint64_t>(arg));\n"
          "    } else {\n"
          "      static_assert(sizeof(T) == 0, \"Unsupported type in FlattenHostArgs\");\n"
          "    }\n"
          "  };\n"
          "  (append_arg(std::forward<Args>(args)), ...);\n"
          "  return buf;\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kFlattenHostArgs:
      output =
          "template<typename... Args>\n"
          "inline std::vector<uint64_t> FlattenHostArgs(Args&&... args) {\n"
          "  std::vector<uint64_t> buf;\n"
          "  auto append_arg = [&](auto&& arg) {\n"
          "    using T = std::decay_t<decltype(arg)>;\n"
          "    if constexpr (std::is_pointer_v<T>) {\n"
          "      buf.push_back(reinterpret_cast<uint64_t>(arg));\n"
          "    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {\n"
          "      for (auto d : arg) buf.push_back(static_cast<uint64_t>(d));\n"
          "    } else if constexpr (std::is_integral_v<T>) {\n"
          "      buf.push_back(static_cast<uint64_t>(arg));\n"
          "    } else {\n"
          "      static_assert(sizeof(T) == 0, \"Unsupported type in FlattenHostArgs\");\n"
          "    }\n"
          "  };\n"
          "  (append_arg(std::forward<Args>(args)), ...);\n"
          "  return buf;\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kScopeGuard:
      output =
          "class ScopeGuard {\n"
          "  public:\n"
          "    ScopeGuard(const ScopeGuard &) = delete;\n"
          "    ScopeGuard &operator=(const ScopeGuard &) = delete;\n"
          "\n"
          "    explicit ScopeGuard(const std::function<void()> &on_exit_scope)\n"
          "        : on_exit_scope_(on_exit_scope) {}\n"
          "\n"
          "    ~ScopeGuard() {\n"
          "      if (on_exit_scope_) {\n"
          "        try {\n"
          "          on_exit_scope_();\n"
          "        } catch (std::bad_function_call &) {\n"
          "        } catch (...) {\n"
          "        }\n"
          "      }\n"
          "    }\n"
          "\n"
          "  private:\n"
          "    std::function<void()> on_exit_scope_;\n"
          "};\n";
      return SUCCESS;
    case StablePartId::kReadBinaryFileToBuffer:
      output =
          "BinaryBuffer ReadBinaryFileToBuffer(const std::string &file_path) {\n"
          "  BinaryBuffer result;\n"
          "  std::ifstream file(file_path, std::ios::binary | std::ios::ate);\n"
          "  if (!file.is_open()) {\n"
          "    return result;\n"
          "  }\n"
          "  std::streamsize file_size = file.tellg();\n"
          "  if (file_size <= 0) {\n"
          "    return result;\n"
          "  }\n"
          "  result.size = static_cast<size_t>(file_size);\n"
          "  result.data = std::make_unique<uint8_t[]>(result.size);\n"
          "  file.seekg(0, std::ios::beg);\n"
          "  file.read(reinterpret_cast<char *>(result.data.get()), result.size);\n"
          "  if (!file.good()) {\n"
          "    file.close();\n"
          "    result.data.reset();\n"
          "    result.size = 0;\n"
          "  }\n"
          "  return result;\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kGenerateJsonFile:
      output =
          "aclError GenerateJsonFile(const AicpuRegisterInfo &register_info, std::string &json_path) {\n"
          "  using namespace std::chrono;\n"
          "  int64_t cur_timestamp = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();\n"
          "  json_path = \"/tmp/temp_ops_info_\" + std::to_string(cur_timestamp) + \".json\";\n"
          "  std::string json_data_format = R\"(\n"
          "{\n"
          "    \"%s\":{\n"
          "        \"opInfo\":{\n"
          "            \"opKernelLib\":\"%s\",\n"
          "            \"kernelSo\":\"%s\",\n"
          "            \"functionName\":\"%s\"\n"
          "        }\n"
          "    }\n"
          "}\n"
          ")\";\n"
          "  char json_data[kMaxJsonFileLen];\n"
          "  auto ret = snprintf_s(json_data, kMaxJsonFileLen, kMaxJsonFileLen - 1U, json_data_format.c_str(),\n"
          "                        register_info.op_type, register_info.op_kernel_lib, register_info.so_name,\n"
          "                        register_info.kernel_name);\n"
          "  OM2_CHK_TRUE(ret >= 0);\n"
          "  std::ofstream ofs(json_path.c_str(), std::ios::trunc);\n"
          "  OM2_CHK_TRUE(ofs);\n"
          "  ofs << json_data;\n"
          "  return ACL_SUCCESS;\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kLoadAndRunExternalApis:
      output =
          "aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, const char **bin_files, const void **bin_data, "
          "size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id) {\n"
          "  if (*model_handle != nullptr) {\n"
          "    return ACL_ERROR_FAILURE;\n"
          "  }\n"
          "  auto *obj = new om2::Om2Model(bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id);\n"
          "  if (obj == nullptr) {\n"
          "    return ACL_ERROR_FAILURE;\n"
          "  }\n"
          "  auto ret = obj->InitResources();\n"
          "  if (ret != ACL_SUCCESS) {\n"
          "    delete obj;\n"
          "    return ret;\n"
          "  }\n"
          "  ret = obj->RegisterKernels();\n"
          "  if (ret != ACL_SUCCESS) {\n"
          "    delete obj;\n"
          "    return ret;\n"
          "  }\n"
          "  ret = obj->Load();\n"
          "  if (ret != ACL_SUCCESS) {\n"
          "    delete obj;\n"
          "    return ret;\n"
          "  }\n"
          "  *model_handle = reinterpret_cast<om2::Om2ModelHandle>(obj);\n"
          "  return ACL_SUCCESS;\n"
          "}\n"
          "\n"
          "aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, "
          "void **input_data, int output_count, void **output_data) {\n"
          "  return static_cast<om2::Om2Model*>(*model_handle)->RunAsync(stream, input_count, input_data, output_count, "
          "output_data);\n"
          "}\n"
          "\n"
          "aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, "
          "void **output_data) {\n"
          "  return static_cast<om2::Om2Model*>(*model_handle)->Run(input_count, input_data, output_count, output_data);\n"
          "}\n"
          "\n"
          "aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle) {\n"
          "  delete static_cast<om2::Om2Model*>(*model_handle);\n"
          "  return ACL_SUCCESS;\n"
          "}\n";
      return SUCCESS;
    case StablePartId::kCreateLabelListForLabelSwitch:
      output = 
              "aclError Om2Model::CreateLabelListForLabelSwitch(uint32_t op_index, std::vector<uint32_t> label_list_indexs)\n"
              "{\n"
              "  label_switch_label_list_[op_index] = nullptr;\n"
              "  uint32_t label_used_size = label_list_indexs.size();\n"
              "  label_used_.resize(label_used_size, nullptr);\n"
              "  for (uint32_t i = 0; i < label_used_size; i++) {\n"
              "    label_used_[i] = label_list_[label_list_indexs[i]];\n"
              "  }\n"
              "  OM2_CHK_STATUS(aclrtCreateLabelList(label_used_.data(), label_used_.size(), &(label_switch_label_list_[op_index])));\n"
              "  return ACL_SUCCESS;\n"
              "}\n";
      return SUCCESS;
    case StablePartId::kCreateLabelListForLabelGotoEx:
      output =
              "aclError Om2Model::CreateLabelListForLabelGotoEx(uint32_t op_index, uint32_t label_index)\n"
              "{\n"
              "  label_goto_ex_label_list_[op_index] = nullptr;\n"
              "  const auto it = label_goto_args_.find(label_index);\n"
              "  if (it != label_goto_args_.cend()) {\n"
              "    label_goto_ex_label_list_[op_index] = it->second.first;\n"
              "  } else {\n"
              "    label_used_.resize(1, nullptr);\n"
              "    label_used_[0] = label_list_[static_cast<size_t>(label_index)];\n"
              "    OM2_CHK_STATUS(aclrtCreateLabelList(label_used_.data(), label_used_.size(), &(label_goto_ex_label_list_[op_index])));\n"
              "    label_goto_args_[label_index] = {label_goto_ex_label_list_[op_index], static_cast<uint32_t>(sizeof(rtLabelDevInfo))};\n"
              "  }\n"
              "  return ACL_SUCCESS;\n"
              "}\n";
      return SUCCESS;
    default:
      output.clear();
      return FAILED;
  }
}
}  // namespace ge
