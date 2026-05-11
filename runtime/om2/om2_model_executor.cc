/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>
#include <fstream>
#include <regex>
#include <sys/syscall.h>
#include <unistd.h>
#include "acl/acl_rt.h"
#include "registry/op_impl_space_registry_v2.h"
#include "framework/runtime/rt_session.h"
#include "runtime/om2_model_executor.h"
#include "common/checker.h"
#include "mmpa/mmpa_api.h"
#include "graph/utils/type_utils.h"
#include "graph_metadef/common/ge_common/util.h"
#include "framework/common/scope_guard.h"
#include "runtime/mem.h"
#include "common/aclrt_malloc_helper.h"
#include "common/helper/om2/json_file.h"
#include "common/compile_profiling/ge_call_wrapper.h"
#include "file_const_loader.h"
#include "om2_external_weight_manager.h"
#include "om2_file_utils.h"
#include "om2_var_manager.h"
#include "zip_archive_reader.h"

namespace gert {
namespace {
constexpr size_t kMaxErrorStringLen = 128U;
constexpr size_t FILE_MAGIC_HEADER_SIZE = 4U;
constexpr uint8_t OM2_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};

using Om2ModelHandle = void *;
using CreateFunc = ge::graphStatus (*)(Om2ModelHandle *, const char **, const void **, size_t *, int, void **,
                                       void *, uint64_t *);
using DestroyFunc = ge::graphStatus (*)(Om2ModelHandle *);
using RunFunc = ge::graphStatus (*)(Om2ModelHandle *, int, void **, int, void **);
using RunAsyncFunc = ge::graphStatus (*)(Om2ModelHandle *, rtStream_t, int, void **, int, void **);

struct RunModelInfo {
  std::string so_file;
  int32_t so_fd = -1;
  ge::JsonFile model_meta_json;
  ge::JsonFile constants_json;
  void *so_handle = nullptr;
  std::string model_name;
  Om2ModelHandle model_handle = nullptr;
  CreateFunc create_func = nullptr;
  DestroyFunc destroy_func = nullptr;
  RunFunc run_func = nullptr;
  RunAsyncFunc run_async_func = nullptr;
};

struct KernelBinInfo {
  std::string file;
  ge::ReadonlyByteBuffer data;
  size_t data_size;
};

struct ClassifiedConstItems {
  std::vector<Om2ConstItem> internal_consts;
  std::vector<Om2ConstItem> combined_consts;
  std::vector<Om2ConstItem> individual_consts;
  size_t max_index = 0U;
};

std::pair<std::string, std::string> ExtractParentDirAndFileName(const std::string &abs_path) {
  if (abs_path.empty()) {
    return {"", ""};
  }
  size_t last_slash = abs_path.find_last_of('/');
  if (last_slash == std::string::npos) {
    return {"", abs_path};
  }
  if (last_slash == 0) {
    return {"/", abs_path.substr(1)};
  }
  return {abs_path.substr(0, last_slash + 1), abs_path.substr(last_slash + 1)};
}

void CloseMemFd(int32_t &fd) {
  if (fd >= 0) {
    (void)mmClose(fd);
    fd = -1;
  }
}

ge::Status CreateSoMemFd(const std::string &file_name, const void *data, const size_t size,
                         std::string &fd_path, int32_t &fd) {
  GE_ASSERT_NOTNULL(data);
  GE_ASSERT_TRUE(size > 0U);
  CloseMemFd(fd);

  const auto short_name = ExtractParentDirAndFileName(file_name).second;
  fd = static_cast<int32_t>(syscall(__NR_memfd_create, short_name.c_str(), 0));
  GE_ASSERT_TRUE(fd >= 0, "[OM2][Create][MemFd] Failed, file=%s", file_name.c_str());
  GE_DISMISSABLE_GUARD(memfd_cleanup, [&]() { CloseMemFd(fd); });

  const auto write_count = mmWrite(fd, const_cast<void *>(data), size);
  GE_ASSERT_TRUE(write_count == static_cast<mmSsize_t>(size),
                 "[OM2][Write][MemFd] Failed, file=%s, size=%zu, write_count=%lld",
                 file_name.c_str(), size, static_cast<long long>(write_count));
  GE_ASSERT_TRUE(lseek(fd, 0, SEEK_SET) >= 0, "[OM2][Seek][MemFd] Failed, file=%s", file_name.c_str());

  fd_path = "/proc/self/fd/" + std::to_string(fd);
  GE_DISMISS_GUARD(memfd_cleanup);
  return ge::SUCCESS;
}

bool IsFileNameEndsWith(const std::string &file_name, const std::string &ext) {
  return file_name.size() >= ext.size() && file_name.compare(file_name.size() - ext.size(), ext.size(), ext) == 0;
}

template <typename T>
ge::Status GetModelJsonValue(const std::string &key, T &out, const ge::JsonFile &json_file) {
  GE_ASSERT_TRUE(json_file.IsValid());
  GE_ASSERT_TRUE(json_file.Get(key, out));
  return ge::SUCCESS;
}

ge::Status SetTensorDesc(ge::JsonFile::json &tensor_array_json, std::vector<ge::TensorDesc> &tensor_desc_array,
                         const bool new_model_desc = false) {
  for (ge::JsonFile::json &tensor : tensor_array_json) {
    ge::JsonFile tensor_obj{tensor};
    ge::TensorDesc tensor_desc;
    std::string name;
    GE_ASSERT_TRUE(tensor_obj.Get("name", name));
    tensor_desc.SetName(name.c_str());
    std::string data_type;
    GE_ASSERT_TRUE(tensor_obj.Get("data_type", data_type));
    tensor_desc.SetDataType(ge::TypeUtils::SerialStringToDataType(data_type));
    std::string format;
    GE_ASSERT_TRUE(tensor_obj.Get("format", format));
    tensor_desc.SetFormat(ge::TypeUtils::SerialStringToFormat(format));
    int64_t size;
    GE_ASSERT_TRUE(tensor_obj.Get("size", size));
    tensor_desc.SetSize(size);
    std::string shape_key = new_model_desc ? "shape_v2" : "shape";
    std::vector<int64_t> shape_dims;
    GE_ASSERT_TRUE(tensor_obj.Get(shape_key, shape_dims));
    const ge::Shape ge_shape(shape_dims);
    tensor_desc.SetShape(ge_shape);
    std::vector<std::pair<int64_t, int64_t>> shape_range;
    GE_ASSERT_TRUE(tensor_obj.Get("shape_range", shape_range));
    tensor_desc.SetShapeRange(shape_range);
    tensor_desc_array.emplace_back(tensor_desc);
  }
  return ge::SUCCESS;
}

uint64_t GetNextSessionId() {
  static std::atomic<uint64_t> atomic_session_id(0);
  return atomic_session_id.fetch_add(1);
}

uint64_t ResolveSessionId(const Om2ModelLoadArg &load_arg) {
  return (load_arg.rt_session == nullptr) ? GetNextSessionId() : load_arg.rt_session->GetSessionId();
}

ge::Status GetOm2MemAndWeightSizeFromArchive(ge::RAIIZipArchive &archive, size_t &work_size,
                                             size_t &internal_weight_size) {
  work_size = 0U;
  internal_weight_size = 0U;
  const auto file_names = archive.ListFiles();
  for (const auto &file_name : file_names) {
    if (IsFileNameEndsWith(file_name, "model_meta.json")) {
      size_t buff_size = 0UL;
      auto buff_data = archive.ExtractToMem(file_name, buff_size);
      GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
      const ge::JsonFile model_meta_json(buff_data.get(), buff_size);
      GE_ASSERT_TRUE(model_meta_json.IsValid());
      GE_ASSERT_SUCCESS(GetModelJsonValue("work_size", work_size, model_meta_json));
      continue;
    }
    if (IsFileNameEndsWith(file_name, "_constants_config.json")) {
      size_t buff_size = 0UL;
      auto buff_data = archive.ExtractToMem(file_name, buff_size);
      GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
      const ge::JsonFile constants_json(buff_data.get(), buff_size);
      GE_ASSERT_TRUE(constants_json.IsValid());
      (void)constants_json.Get("internal_weight_size", internal_weight_size);
      continue;
    }
  }
  return ge::SUCCESS;
}

ge::Status RtMallocBuffer(size_t size, void *&ptr) {
  ptr = nullptr;
  if (size == 0U) {
    return ge::SUCCESS;
  }
  const auto rt_ret = ge::AclrtMalloc(&ptr, size, RT_MEMORY_HBM, 0);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(ge::FAILED, "[OM2][Alloc] aclrtMalloc failed, size=%zu, rt_ret=%u", size, rt_ret);
    return ge::FAILED;
  }
  return ge::SUCCESS;
}

ge::Status RtMemcpyH2D(void *dst, size_t dst_size, const void *src, size_t size) {
  if (size == 0U) {
    return ge::SUCCESS;
  }
  const auto rt_ret = aclrtMemcpy(dst, dst_size, src, size, ACL_MEMCPY_HOST_TO_DEVICE);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(ge::FAILED, "[OM2][Memcpy] rtMemcpy H2D failed, size=%zu, rt_ret=%u", size, rt_ret);
    return ge::FAILED;
  }
  return ge::SUCCESS;
}

ge::Status ParseConstItems(const ge::JsonFile &constants_json, std::vector<Om2ConstItem> &const_items,
                           size_t &internal_weight_size) {
  const_items.clear();
  internal_weight_size = 0U;
  if (!constants_json.IsValid()) {
    return ge::SUCCESS;
  }
  (void)constants_json.Get("internal_weight_size", internal_weight_size);
  ge::JsonFile::json consts_json;
  if (!constants_json.Get("consts", consts_json) || !consts_json.is_object()) {
    return ge::SUCCESS;
  }
  for (auto iter = consts_json.begin(); iter != consts_json.end(); ++iter) {
    ge::JsonFile const_item_json(iter.value());
    Om2ConstItem const_item;
    GE_ASSERT_TRUE(const_item_json.Get("index", const_item.index));
    GE_ASSERT_TRUE(const_item_json.Get("type", const_item.type));
    if (const_item.type == "INTERNAL") {
      (void)const_item_json.Get("file_name", const_item.file_name);
    } else {
      GE_ASSERT_TRUE(const_item_json.Get("file_name", const_item.file_name));
      GE_ASSERT_TRUE(!const_item.file_name.empty());
    }
    GE_ASSERT_TRUE(const_item_json.Get("offset", const_item.offset));
    GE_ASSERT_TRUE(const_item_json.Get("size", const_item.size));
    const_items.emplace_back(const_item);
  }
  return ge::SUCCESS;
}

ge::Status ClassifyConstItems(const std::vector<Om2ConstItem> &const_items, ClassifiedConstItems &classified_items) {
  classified_items = ClassifiedConstItems();
  for (const auto &const_item : const_items) {
    classified_items.max_index = std::max(classified_items.max_index, const_item.index);
    if (const_item.type == "INTERNAL") {
      classified_items.internal_consts.emplace_back(const_item);
      continue;
    }
    if (const_item.type == "COMBINED") {
      classified_items.combined_consts.emplace_back(const_item);
      continue;
    }
    if (const_item.type == "INDIVIDUAL") {
      classified_items.individual_consts.emplace_back(const_item);
      continue;
    }
    GELOGE(ge::FAILED, "[OM2][Check] Unsupported const type, type=%s", const_item.type.c_str());
    return ge::FAILED;
  }
  return ge::SUCCESS;
}
}  // namespace

class Om2ModelExecutor::Impl {
 public:
  ge::Status ParseModel(ge::ModelData &model_data, ge::ReadonlyByteBuffer &weight_buf,
                        std::vector<KernelBinInfo> &kernel_bin_info) {
    has_model_ = false;
    CloseMemFd(run_model_info_.so_fd);
    run_model_info_ = RunModelInfo();
    weight_buf.reset(nullptr);
    kernel_bin_info.clear();

    ge::RAIIZipArchive archive(static_cast<uint8_t *>(model_data.model_data), model_data.model_len);
    if (!archive.IsGood()) {
      GELOGE(ACL_ERROR_GE_PARAM_INVALID,
             "[Check][Param] Invalid om2 model buffer or model length, archive init failed.");
      return ACL_ERROR_GE_PARAM_INVALID;
    }
    const auto file_names = archive.ListFiles();

    for (const auto &file_name : file_names) {
      if (IsFileNameEndsWith(file_name, "model_meta.json")) {
        size_t buff_size = 0UL;
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
        run_model_info_.model_meta_json = ge::JsonFile(buff_data.get(), buff_size);
        GE_ASSERT_TRUE(run_model_info_.model_meta_json.IsValid());
        GE_ASSERT_SUCCESS(GetModelJsonValue("name", run_model_info_.model_name, run_model_info_.model_meta_json));
        has_model_ = true;
        break;
      }
    }
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_SUCCESS(ParseArchiveFiles(archive, file_names, weight_buf, kernel_bin_info));
    GE_ASSERT_TRUE(!run_model_info_.so_file.empty(), "[OM2] Om2 compiled so not found, need to check .om2 file.");
    return ge::SUCCESS;
  }

 private:
  ge::Status ParseArchiveFiles(ge::RAIIZipArchive &archive, const std::vector<std::string> &file_names,
                               ge::ReadonlyByteBuffer &weight_buf,
                               std::vector<KernelBinInfo> &kernel_bin_info) {
    for (const auto &file_name : file_names) {
      if (IsFileNameEndsWith(file_name, "manifest.json") || IsFileNameEndsWith(file_name, "model_meta.json")) {
        continue;
      }

      size_t buff_size = 0UL;
      if (std::regex_search(file_name, std::regex("constant_\\d+$"))) {
        GELOGI("file name:%s is constant file.", file_name.c_str());
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
        weight_buf = std::move(buff_data);
        continue;
      }
      if (IsFileNameEndsWith(file_name, "_constants_config.json")) {
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
        run_model_info_.constants_json = ge::JsonFile(buff_data.get(), buff_size);
        GE_ASSERT_TRUE(run_model_info_.constants_json.IsValid());
        continue;
      }
      if (IsFileNameEndsWith(file_name, ".o")) {
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
        auto file_info = ExtractParentDirAndFileName(file_name);
        kernel_bin_info.push_back({file_info.second, std::move(buff_data), buff_size});
        continue;
      }
      if (IsFileNameEndsWith(file_name, "lib" + run_model_info_.model_name + "_om2.so")) {
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
        GE_ASSERT_SUCCESS(CreateSoMemFd(file_name, buff_data.get(), buff_size, run_model_info_.so_file,
                                        run_model_info_.so_fd));
      }
    }
    return ge::SUCCESS;
  }

 public:

  ge::Status LoadSharedObject() {
    GELOGI("[OM2] Begin loading so file %s", run_model_info_.so_file.c_str());
    GE_ASSERT_TRUE(!run_model_info_.so_file.empty());
    run_model_info_.so_handle = mmDlopen(run_model_info_.so_file.c_str(), MMPA_RTLD_NOW);
    if (run_model_info_.so_handle == nullptr) {
      const char_t *error = mmDlerror();
      error = (error == nullptr) ? "" : error;
      GELOGE(ge::FAILED, "[OM2][Invoke][DlOpen] Failed to  load so, path = [%s], error = [%s]",
             run_model_info_.so_file.c_str(), error);
      return ge::FAILED;
    }
    return ge::SUCCESS;
  }

  ge::Status ResolveSymbols() {
    GE_ASSERT_TRUE(run_model_info_.so_handle != nullptr);
    run_model_info_.create_func = reinterpret_cast<CreateFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelCreate"));
    GE_ASSERT_NOTNULL(run_model_info_.create_func);
    run_model_info_.destroy_func =
        reinterpret_cast<DestroyFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelDestroy"));
    GE_ASSERT_NOTNULL(run_model_info_.destroy_func);
    run_model_info_.run_func = reinterpret_cast<RunFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelRun"));
    GE_ASSERT_NOTNULL(run_model_info_.run_func);
    run_model_info_.run_async_func =
        reinterpret_cast<RunAsyncFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelRunAsync"));
    GE_ASSERT_NOTNULL(run_model_info_.run_async_func);
    return ge::SUCCESS;
  }

  ge::Status CreateModel(ge::ModelData &model_data, ge::ReadonlyByteBuffer &weight_buf,
                         std::vector<KernelBinInfo> &kernel_bin_info, const Om2ModelLoadArg &load_arg,
                         uint64_t session_id) {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_TRUE(load_arg.device_id >= 0, "[OM2][Check] Invalid device id.");
    device_id_ = load_arg.device_id;
    std::vector<const char *> bin_files(kernel_bin_info.size());
    std::vector<const void *> bin_data(kernel_bin_info.size());
    std::vector<size_t> bin_sizes(kernel_bin_info.size());
    std::vector<void *> constants;
    void *work_ptr = nullptr;
    for (auto i = 0U; i < kernel_bin_info.size(); ++i) {
      bin_files[i] = kernel_bin_info[i].file.c_str();
      bin_data[i] = kernel_bin_info[i].data.get();
      bin_sizes[i] = kernel_bin_info[i].data_size;
    }
    session_id_ = session_id;
    GE_ASSERT_SUCCESS(PrepareWorkPtr(load_arg, work_ptr));
    GE_ASSERT_SUCCESS(PrepareConstants(model_data, weight_buf, load_arg, constants));
    GE_ASSERT_SUCCESS(run_model_info_.create_func(&run_model_info_.model_handle, bin_files.data(), bin_data.data(),
                                                  bin_sizes.data(), static_cast<int>(bin_data.size()),
                                                  constants.empty() ? nullptr : constants.data(), work_ptr,
                                                  &session_id_));
    weight_buf.reset(nullptr);
    kernel_bin_info.clear();
    return ge::GRAPH_SUCCESS;
  }

  ge::Status Run(std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_NOTNULL(run_model_info_.run_func);
    GE_ASSERT_NOTNULL(run_model_info_.model_handle);
    GE_ASSERT_SUCCESS(run_model_info_.run_func(&run_model_info_.model_handle, inputs.size(),
                                               reinterpret_cast<void **>(inputs.data()), outputs.size(),
                                               reinterpret_cast<void **>(outputs.data())));
    return ge::GRAPH_SUCCESS;
  }

  ge::Status RunAsync(void *const stream, std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_NOTNULL(run_model_info_.run_async_func);
    GE_ASSERT_NOTNULL(run_model_info_.model_handle);
    GE_ASSERT_SUCCESS(run_model_info_.run_async_func(&run_model_info_.model_handle, stream, inputs.size(),
                                                     reinterpret_cast<void **>(inputs.data()), outputs.size(),
                                                     reinterpret_cast<void **>(outputs.data())));
    return ge::GRAPH_SUCCESS;
  }

  ge::Status GetDynamicBatchInfo(std::vector<std::vector<int64_t>> &dynamic_batch_info, int32_t &dynamic_type) const {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_SUCCESS(GetModelJsonValue("dynamic_batch_info", dynamic_batch_info, run_model_info_.model_meta_json));
    GE_ASSERT_SUCCESS(GetModelJsonValue("dynamic_type", dynamic_type, run_model_info_.model_meta_json));
    return ge::SUCCESS;
  }

  ge::Status GetModelAttrs(std::vector<std::string> &dynamic_output_shape) const {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_SUCCESS(GetModelJsonValue("dynamic_output_shape", dynamic_output_shape, run_model_info_.model_meta_json));
    return ge::SUCCESS;
  }

  ge::Status GetModelDescInfo(std::vector<ge::TensorDesc> &input_desc, std::vector<ge::TensorDesc> &output_desc,
                              const bool new_model_desc) const {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_TRUE(run_model_info_.model_meta_json.IsValid());
    ge::JsonFile::json inputs;
    GE_ASSERT_TRUE(run_model_info_.model_meta_json.Get("inputs", inputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(inputs, input_desc, new_model_desc));
    ge::JsonFile::json outputs;
    GE_ASSERT_TRUE(run_model_info_.model_meta_json.Get("outputs", outputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(outputs, output_desc));
    return ge::SUCCESS;
  }

  ge::Status GetUserDesignateShapeOrder(std::vector<std::string> &user_designate_shape_order) const {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_SUCCESS(
        GetModelJsonValue("user_designate_shape_order", user_designate_shape_order, run_model_info_.model_meta_json));
    return ge::SUCCESS;
  }

  void Cleanup() {
    if (run_model_info_.destroy_func != nullptr && run_model_info_.model_handle != nullptr) {
      const auto destroy_ret = run_model_info_.destroy_func(&run_model_info_.model_handle);
      if (destroy_ret != ge::GRAPH_SUCCESS) {
        GELOGI("[OM2] Releasing resources failed, so file: %s", run_model_info_.so_file.c_str());
      }
    } else {
      GELOGI("[OM2] Destroy func not found or model not created, so file: %s", run_model_info_.so_file.c_str());
    }
    if (run_model_info_.so_handle != nullptr) {
      if (mmDlclose(run_model_info_.so_handle) != 0) {
        const char_t *error = mmDlerror();
        error = (error == nullptr) ? "" : error;
        GELOGI("[OM2][Invoke][Dlclose] failed. path = %s, error = %s", run_model_info_.so_file.c_str(), error);
      }
      run_model_info_.so_handle = nullptr;
    }
    CloseMemFd(run_model_info_.so_fd);
    ReleaseOwnedMemory();
    // Currently only the non-shared OM2 path is supported, so the executor releases session-level managers here.
    // When bundle model or shared RtSession is supported, add a condition to avoid releasing shared resources.
    Om2VarManagerPool::Instance().RemoveManager(session_id_);
    Om2ExternalWeightManagerPool::Instance().RemoveManager(session_id_);
  }

 private:
  ge::Status PrepareWorkPtr(const Om2ModelLoadArg &load_arg, void *&work_ptr) {
    size_t required_work_size = 0U;
    GE_ASSERT_SUCCESS(GetModelJsonValue("work_size", required_work_size, run_model_info_.model_meta_json));
    if (load_arg.work_ptr != nullptr) {
      GE_ASSERT_TRUE(load_arg.work_size >= required_work_size, "[OM2][Check] Invalid external work_ptr size.");
      work_ptr = load_arg.work_ptr;
      return ge::SUCCESS;
    }
    void *inner_work_ptr = nullptr;
    GE_ASSERT_SUCCESS(RtMallocBuffer(required_work_size, inner_work_ptr));
    if (inner_work_ptr != nullptr) {
      owned_buffers_.push_back(inner_work_ptr);
    }
    work_ptr = inner_work_ptr;
    return ge::SUCCESS;
  }

  ge::Status PrepareConstants(const ge::ModelData &model_data, const ge::ReadonlyByteBuffer &weight_buf,
                              const Om2ModelLoadArg &load_arg, std::vector<void *> &constants) {
    std::vector<Om2ConstItem> const_items;
    size_t internal_weight_size = 0U;
    GE_ASSERT_SUCCESS(ParseConstItems(run_model_info_.constants_json, const_items, internal_weight_size));
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    ClassifiedConstItems classified_items;
    GE_ASSERT_SUCCESS(ClassifyConstItems(const_items, classified_items));
    constants.resize(classified_items.max_index + 1U, nullptr);
    std::map<std::string, ge::FileConstantMem> user_file_const_mems;
    GE_ASSERT_SUCCESS(BuildUserFileConstMemMap(load_arg.file_constant_mems, user_file_const_mems));
    GE_ASSERT_SUCCESS(PrepareInternalConsts(weight_buf, load_arg, classified_items.internal_consts,
                                            internal_weight_size, constants));
    GE_ASSERT_SUCCESS(
        PrepareCombinedConsts(model_data, user_file_const_mems, classified_items.combined_consts, constants));
    GE_ASSERT_SUCCESS(PrepareIndividualConsts(model_data, user_file_const_mems, classified_items.individual_consts,
                                              constants));
    return ge::SUCCESS;
  }

  ge::Status PrepareInternalConsts(const ge::ReadonlyByteBuffer &weight_buf, const Om2ModelLoadArg &load_arg,
                                   const std::vector<Om2ConstItem> &const_items, size_t internal_weight_size,
                                   std::vector<void *> &constants) {
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    GE_ASSERT_TRUE(weight_buf != nullptr, "[OM2][Check] Missing internal host weight buffer.");
    const void *host_weight_base = weight_buf.get();
    void *device_weight_base = load_arg.weight_ptr;
    if (device_weight_base != nullptr) {
      GE_ASSERT_TRUE(load_arg.weight_size >= internal_weight_size, "[OM2][Check] Invalid external device weight size.");
    } else {
      GE_ASSERT_SUCCESS(RtMallocBuffer(internal_weight_size, device_weight_base));
      if (device_weight_base != nullptr) {
        owned_buffers_.push_back(device_weight_base);
      }
    }
    GE_ASSERT_SUCCESS(RtMemcpyH2D(device_weight_base, internal_weight_size, host_weight_base, internal_weight_size));
    auto *device_weight_bytes = static_cast<uint8_t *>(device_weight_base);
    for (const auto &const_item : const_items) {
      GE_ASSERT_TRUE((const_item.offset + const_item.size) <= internal_weight_size,
                     "[OM2][Check] Invalid INTERNAL const offset or size.");
      constants[const_item.index] = device_weight_bytes + const_item.offset;
    }
    return ge::SUCCESS;
  }

  ge::Status PrepareCombinedConsts(const ge::ModelData &model_data,
                                   const std::map<std::string, ge::FileConstantMem> &user_file_const_mems,
                                   const std::vector<Om2ConstItem> &const_items, std::vector<void *> &constants) {
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    std::string weight_dir;
    GE_ASSERT_SUCCESS(ResolveFileConstWeightDir(model_data, weight_dir));
    FileConstContext file_const_ctx;
    file_const_ctx.weight_dir = weight_dir;
    file_const_ctx.user_file_const_mems = &user_file_const_mems;
    file_const_ctx.owned_buffers = &owned_buffers_;
    file_const_ctx.session_id = session_id_;
    file_const_ctx.device_id = device_id_;
    GE_ASSERT_SUCCESS(gert::PrepareCombinedConsts(const_items, file_const_ctx, constants));
    return ge::SUCCESS;
  }

  ge::Status PrepareIndividualConsts(const ge::ModelData &model_data,
                                     const std::map<std::string, ge::FileConstantMem> &user_file_const_mems,
                                     const std::vector<Om2ConstItem> &const_items, std::vector<void *> &constants) {
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    std::string weight_dir;
    GE_ASSERT_SUCCESS(ResolveFileConstWeightDir(model_data, weight_dir));
    std::mutex owned_buffers_mutex;
    FileConstContext file_const_ctx;
    file_const_ctx.weight_dir = weight_dir;
    file_const_ctx.user_file_const_mems = &user_file_const_mems;
    file_const_ctx.owned_buffers = &owned_buffers_;
    file_const_ctx.owned_buffers_mutex = &owned_buffers_mutex;
    file_const_ctx.session_id = session_id_;
    file_const_ctx.device_id = device_id_;

    GE_ASSERT_SUCCESS(gert::PrepareIndividualConsts(const_items, file_const_ctx, device_id_, constants));
    return ge::SUCCESS;
  }

  void ReleaseOwnedMemory() {
    for (auto iter = owned_buffers_.rbegin(); iter != owned_buffers_.rend(); ++iter) {
      if (*iter != nullptr) {
        (void)aclrtFree(*iter);
      }
    }
    owned_buffers_.clear();
  }

  RunModelInfo run_model_info_;
  std::vector<void *> owned_buffers_;
  int32_t device_id_ = -1;
  uint64_t session_id_ = 0U;
  bool has_model_ = false;
};

Om2ModelExecutor::Om2ModelExecutor() : impl_(std::make_unique<Impl>()) {}

Om2ModelExecutor::~Om2ModelExecutor() {
  if (impl_) {
    impl_->Cleanup();
  }
}

ge::Status Om2ModelExecutor::Load(ge::ModelData &model_data, const Om2ModelLoadArg &load_arg,
                                  const uint64_t session_id) const {
  ge::ReadonlyByteBuffer weight_buf;
  std::vector<KernelBinInfo> kernel_bin_info;
  GE_CHK_STATUS_RET(impl_->ParseModel(model_data, weight_buf, kernel_bin_info),
                    "[OM2][Load] Parse model failed.");
  GE_ASSERT_SUCCESS(impl_->LoadSharedObject());
  GE_ASSERT_SUCCESS(impl_->ResolveSymbols());
  GE_ASSERT_SUCCESS(impl_->CreateModel(model_data, weight_buf, kernel_bin_info, load_arg, session_id));
  return ge::SUCCESS;
}

ge::Status Om2ModelExecutor::Run(std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) const {
  return impl_->Run(inputs, outputs);
}

ge::Status Om2ModelExecutor::RunAsync(void *const stream, std::vector<gert::Tensor *> &inputs,
                                      std::vector<gert::Tensor *> &outputs) const {
  return impl_->RunAsync(stream, inputs, outputs);
}

ge::Status Om2ModelExecutor::GetModelDescInfo(std::vector<ge::TensorDesc> &input_desc,
                                              std::vector<ge::TensorDesc> &output_desc, bool new_model_desc) const {
  return impl_->GetModelDescInfo(input_desc, output_desc, new_model_desc);
}

ge::Status Om2ModelExecutor::GetModelAttrs(std::vector<std::string> &dynamic_output_shape) const {
  return impl_->GetModelAttrs(dynamic_output_shape);
}

ge::Status Om2ModelExecutor::GetDynamicBatchInfo(std::vector<std::vector<int64_t>> &dynamic_batch_info,
                                                 int32_t &dynamic_type) const {
  return impl_->GetDynamicBatchInfo(dynamic_batch_info, dynamic_type);
}

ge::Status Om2ModelExecutor::GetUserDesignateShapeOrder(std::vector<std::string> &user_designate_shape_order) const {
  return impl_->GetUserDesignateShapeOrder(user_designate_shape_order);
}

ge::Status LoadOm2DataFromFile(const std::string &model_path, ge::ModelData &model_data) {
  GELOGI("Begin to load om2 model data from file, path: [%s]", model_path.c_str());
  const std::string file_path = ge::om2::RealPath(model_path.c_str());
  if (file_path.empty()) {
    REPORT_PREDEFINED_ERR_MSG(
        "E13026", std::vector<const char_t *>({"pathname", "reason"}),
        std::vector<const char_t *>({model_path.c_str(), "It is not a real path. Please check your model path."}));
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID,
           "[Call][RealPath] File path is invalid. Please check your text file '%s'.", model_path.c_str());
    return ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID;
  }

  std::ifstream fs(file_path, std::ios::binary);
  if (!fs.is_open()) {
    std::array<char_t, kMaxErrorStringLen + 1U> err_buf = {};
    const auto err_msg = mmGetErrorFormatMessage(mmGetErrorCode(), &err_buf[0], kMaxErrorStringLen);
    const std::string reason = ge::FormatErrnoReason(mmGetErrorCode(), err_msg);
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID, "[Open][File]Failed, file %s, error %s", model_path.c_str(), err_msg);
    REPORT_INNER_ERR_MSG("E19999", "Open file %s failed, reason:%s", model_path.c_str(), reason.c_str());
    return ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID;
  }

  (void)fs.seekg(0, std::ifstream::end);
  const int64_t len = fs.tellg();
  GE_ASSERT_TRUE(len > 1U);
  (void)fs.seekg(0, std::ifstream::beg);

  auto *buffer = new (std::nothrow) char_t[static_cast<size_t>(len)];
  if (buffer == nullptr) {
    GELOGE(ge::FAILED, "[Alloc][Mem] Failed to alloc memory for om2, size: %lld", len);
    return ge::FAILED;
  }

  (void)fs.read(buffer, len);

  model_data.model_data = buffer;
  model_data.model_len = len;
  model_data.om_path = file_path;
  GELOGI("Load om2 model data success, path: %s, size: %zu", model_path.c_str(), static_cast<size_t>(len));

  return ge::SUCCESS;
}

std::unique_ptr<Om2ModelExecutor> LoadOm2ExecutorFromData(ge::ModelData &model_data, const Om2ModelLoadArg &load_arg,
                                                          ge::Status &error_code) {
  auto executor = std::unique_ptr<Om2ModelExecutor>(new (std::nothrow) Om2ModelExecutor());
  if (executor == nullptr) {
    error_code = ge::FAILED;
    GELOGE(ge::FAILED, "Constructing Om2ModelExecutor failed.");
    return executor;
  }
  const uint64_t session_id = ResolveSessionId(load_arg);
  error_code = executor->Load(model_data, load_arg, session_id);
  GE_ASSERT_SUCCESS(error_code);
  return executor;
}

ge::Status GetOm2MemAndWeightSize(const std::string &model_path, size_t &work_size, size_t &internal_weight_size) {
  ge::ModelData model_data;
  GE_CHK_STATUS_RET(LoadOm2DataFromFile(model_path, model_data), "[OM2][Query] Load model data from file failed.");
  std::shared_ptr<void> data_guarder(model_data.model_data, [](const void *const p) {
    if (p != nullptr) {
      delete[] static_cast<const uint8_t *>(p);
    }
  });
  ge::RAIIZipArchive archive(static_cast<uint8_t *>(model_data.model_data), model_data.model_len);
  GE_ASSERT_TRUE(archive.IsGood());
  GE_ASSERT_SUCCESS(GetOm2MemAndWeightSizeFromArchive(archive, work_size, internal_weight_size));
  return ge::SUCCESS;
}

ge::Status GetOm2MemAndWeightSize(const void *model_data, size_t model_size, size_t &work_size,
                                  size_t &internal_weight_size) {
  ge::RAIIZipArchive archive(static_cast<const uint8_t *>(model_data), model_size);
  GE_ASSERT_TRUE(archive.IsGood());
  GE_ASSERT_SUCCESS(GetOm2MemAndWeightSizeFromArchive(archive, work_size, internal_weight_size));
  return ge::SUCCESS;
}

ge::Status IsOm2Model(const void *data, size_t size, bool &is_support) {
  if (data == nullptr) {
    REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                                    std::vector<const char *>({"data", "nullptr", "Model data cannot be nullptr."}));
    GELOGE(ACL_ERROR_GE_PARAM_INVALID, "[Check][Param] Invalid om2 model. Model data cannot be nullptr.");
    return ACL_ERROR_GE_PARAM_INVALID;
  }

  if (size < FILE_MAGIC_HEADER_SIZE) {
    const std::string err_msg =
        "Model data size must be greater than or equal to " + std::to_string(FILE_MAGIC_HEADER_SIZE);
    REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                              std::vector<const char *>({"size", std::to_string(size).c_str(), err_msg.c_str()}));
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID,
           "[Check][Param] Invalid om2 model. Model data size %zu must be greater than or equal to %zu.", size,
           FILE_MAGIC_HEADER_SIZE);
    return ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID;
  }

  is_support = std::memcmp(data, OM2_MAGIC, FILE_MAGIC_HEADER_SIZE) == 0;
  return ge::SUCCESS;
}

ge::Status IsOm2Model(const char *file_path, bool &is_support) {
  const std::string real_path = ge::om2::RealPath(file_path);
  if (real_path.empty()) {
    std::array<char_t, kMaxErrorStringLen + 1U> err_buf = {};
    const auto err_msg = mmGetErrorFormatMessage(mmGetErrorCode(), err_buf.data(), kMaxErrorStringLen);
    std::string reason = ge::FormatErrnoReason(mmGetErrorCode(), err_msg);
    REPORT_PREDEFINED_ERR_MSG("E13000", std::vector<const char *>({"patch", "errmsg"}),
                              std::vector<const char *>({file_path, reason.c_str()}));
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID, "[Check][Param]Model file path %s is invalid", file_path);
    return ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID;
  }

  std::ifstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    std::array<char_t, kMaxErrorStringLen + 1U> err_buf = {};
    const auto err_msg = mmGetErrorFormatMessage(mmGetErrorCode(), &err_buf[0], kMaxErrorStringLen);
    const std::string reason = ge::FormatErrnoReason(mmGetErrorCode(), err_msg);
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID, "[Open][File]Failed, file %s, error %s", file_path, err_msg);
    REPORT_INNER_ERR_MSG("E19999", "Open file %s failed, reason:%s", file_path, reason.c_str());
    return ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID;
  }

  (void)file.seekg(0, std::ifstream::end);
  const size_t len = static_cast<size_t>(file.tellg());
  (void)file.seekg(0, std::ifstream::beg);
  if (len < FILE_MAGIC_HEADER_SIZE) {
    const std::string reason = "Invalid om2 file. The model data size " + std::to_string(len) + " is smaller than " +
                               std::to_string(FILE_MAGIC_HEADER_SIZE) + ".";
    (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                                    std::vector<const char *>({"file_path", file_path, reason.c_str()}));
    GELOGE(ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID,
           "[Check][Param] Invalid om2 model. Model data size %" PRIu64 " must be greater than or equal to %zu.", len,
           FILE_MAGIC_HEADER_SIZE);
    return ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID;
  }

  uint8_t magic[FILE_MAGIC_HEADER_SIZE] = {};
  file.read(reinterpret_cast<char *>(magic), FILE_MAGIC_HEADER_SIZE);
  const auto read_len = static_cast<size_t>(file.gcount());

  GE_ASSERT_SUCCESS(IsOm2Model(magic, read_len, is_support));
  return ge::SUCCESS;
}
}  // namespace gert
