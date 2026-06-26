/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cinttypes>
#include <string>
#include <fstream>
#include <regex>
#include <sys/syscall.h>
#include <unistd.h>
#include "acl/acl_rt.h"
#include "registry/op_impl_space_registry_v2.h"
#include "framework/runtime/rt_session.h"
#include "framework/runtime/dump/model_dump_manager.h"
#include "framework/common/framework_types_internal.h"
#include "runtime/om2_model_executor.h"
#include "common/checker.h"
#include "mmpa/mmpa_api.h"
#include "../../inc/framework/runtime/om2_context.h"
#include "graph/utils/type_utils_inner.h"
#include "graph_metadef/common/ge_common/util.h"
#include "rt_external_mem.h"
#include "common/helper/om2/json_file.h"
#include "common/compile_profiling/ge_call_wrapper.h"
#include "file_const_loader.h"
#include "om2_external_weight_manager.h"
#include "om2_file_utils.h"
#include "om2_malloc_helper.h"
#include "om2_var_manager.h"
#include "zip_archive_reader.h"

namespace gert {
namespace {
constexpr size_t kMaxErrorStringLen = 128U;
constexpr size_t FILE_MAGIC_HEADER_SIZE = 4U;
constexpr uint8_t OM2_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};

using Om2ModelHandle = void *;
using CreateFunc = ge::graphStatus (*)(Om2ModelHandle *, rtModel_t *, const char **, const void **, size_t *, int,
                                       void **, void *, uint64_t *, uint32_t, void *);
using LoadFunc = ge::graphStatus (*)(Om2ModelHandle *);
using DestroyFunc = ge::graphStatus (*)(Om2ModelHandle *);
using RunFunc = ge::graphStatus (*)(Om2ModelHandle *, int, void **, int, void **, int32_t streamSyncTimeout);
using RunAsyncFunc = ge::graphStatus (*)(Om2ModelHandle *, rtStream_t, int, void **, int, void **);

struct RunModelInfo {
  std::string so_file;
  int32_t so_fd = -1;
  ge::JsonFile op_attr_json;
  void *so_handle = nullptr;
  std::string model_name;
  std::string root_graph_name;
  Om2ModelHandle model_handle = nullptr;
  rtModel_t rt_model_handle = nullptr;
  CreateFunc create_func = nullptr;
  LoadFunc load_func = nullptr;
  DestroyFunc destroy_func = nullptr;
  RunFunc run_func = nullptr;
  RunAsyncFunc run_async_func = nullptr;
};

struct ModelMetaInfo {
  size_t work_size = 0U;
  std::vector<ge::Om2TensorDesc> input_desc;
  std::vector<ge::Om2TensorDesc> output_desc;
  std::vector<ge::Om2TensorDesc> input_desc_v2;
  std::vector<ge::Om2TensorDesc> output_desc_v2;
  std::vector<std::vector<int64_t>> dynamic_batch_info;
  int32_t dynamic_type = 0;
  std::vector<std::string> dynamic_output_shape;
  std::vector<std::string> user_designate_shape_order;
  std::vector<std::vector<int64_t>> origin_input_dims;  // 原始shape（含-1标识动态轴）
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

ge::Status CreateSoMemFd(const std::string &file_name, const void *data, const size_t size, std::string &fd_path,
                         int32_t &fd) {
  GE_ASSERT_NOTNULL(data);
  GE_ASSERT_TRUE(size > 0U);
  CloseMemFd(fd);

  const auto short_name = ExtractParentDirAndFileName(file_name).second;
  fd = static_cast<int32_t>(syscall(__NR_memfd_create, short_name.c_str(), 0));
  GE_ASSERT_TRUE(fd >= 0, "[OM2][Create][MemFd] Failed, file=%s", file_name.c_str());
  GE_DISMISSABLE_GUARD(memfd_cleanup, [&fd]() { CloseMemFd(fd); });

  const auto write_count = mmWrite(fd, const_cast<void *>(data), size);
  GE_ASSERT_TRUE(write_count == static_cast<mmSsize_t>(size),
                 "[OM2][Write][MemFd] Failed, file=%s, size=%zu, write_count=%lld", file_name.c_str(), size,
                 static_cast<long long>(write_count));
  GE_ASSERT_TRUE(lseek(fd, 0, SEEK_SET) >= 0, "[OM2][Seek][MemFd] Failed, file=%s", file_name.c_str());

  fd_path = "/proc/" + std::to_string(getpid()) + "/fd/" + std::to_string(fd);
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

ge::Status SetTensorDesc(ge::JsonFile::json &tensor_array_json, std::vector<ge::Om2TensorDesc> &tensor_desc_array,
                         const bool new_model_desc = false) {
  for (ge::JsonFile::json &tensor : tensor_array_json) {
    ge::JsonFile tensor_obj{tensor};
    ge::Om2TensorDesc tensor_desc;
    std::string name;
    GE_ASSERT_TRUE(tensor_obj.Get("name", name));
    tensor_desc.SetName(name);
    std::string data_type;
    GE_ASSERT_TRUE(tensor_obj.Get("data_type", data_type));
    tensor_desc.SetDataType(ge::TypeUtilsInner::SerialStringToDataType(data_type));
    std::string format;
    GE_ASSERT_TRUE(tensor_obj.Get("format", format));
    tensor_desc.SetFormat(ge::TypeUtilsInner::SerialStringToFormat(format));
    int64_t size;
    GE_ASSERT_TRUE(tensor_obj.Get("size", size));
    tensor_desc.SetSize(static_cast<size_t>(size));
    std::string shape_key = new_model_desc ? "shape_v2" : "shape";
    std::vector<int64_t> shape_dims;
    GE_ASSERT_TRUE(tensor_obj.Get(shape_key, shape_dims));
    tensor_desc.SetShape(shape_dims);
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

ge::Status GetOm2ModelMetadataFromArchive(ge::RAIIZipArchive &archive, std::vector<ge::Om2TensorDesc> &input_desc,
                                          std::vector<ge::Om2TensorDesc> &input_desc_v2,
                                          std::vector<ge::Om2TensorDesc> &output_desc,
                                          std::vector<ge::Om2TensorDesc> &output_desc_v2) {
  const auto file_names = archive.ListFiles();
  for (const auto &file_name : file_names) {
    if (!IsFileNameEndsWith(file_name, "model_meta.json")) {
      continue;
    }
    size_t buff_size = 0UL;
    auto buff_data = archive.ExtractToMem(file_name, buff_size);
    GE_ASSERT_TRUE(buff_data != nullptr && buff_size != 0U);
    const ge::JsonFile model_meta_json(buff_data.get(), buff_size);
    GE_ASSERT_TRUE(model_meta_json.IsValid());

    ge::JsonFile::json inputs;
    GE_ASSERT_TRUE(model_meta_json.Get("inputs", inputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(inputs, input_desc));
    GE_ASSERT_SUCCESS(SetTensorDesc(inputs, input_desc_v2, true));

    ge::JsonFile::json outputs;
    GE_ASSERT_TRUE(model_meta_json.Get("outputs", outputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(outputs, output_desc));
    output_desc_v2 = output_desc;
    return ge::SUCCESS;
  }
  GE_ASSERT_TRUE(false, "[OM2] model_meta.json not found in archive.");
  return ge::FAILED;
}

ge::Status RtMallocBuffer(size_t size, void *&ptr) {
  ptr = nullptr;
  if (size == 0U) {
    return ge::SUCCESS;
  }
  const auto rt_ret = Om2Malloc(&ptr, size, RT_MEMORY_HBM, 0);
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

void ParseOpAttrJsonToMapInternal(const ge::JsonFile &op_attr_json,
                                  std::map<std::string, std::map<std::string, std::string>> &attr_map) {
  attr_map.clear();
  if (!op_attr_json.IsValid()) {
    return;
  }

  const auto &json_data = op_attr_json.Raw();
  if (!json_data.is_object()) {
    GELOGW("[OM2] op_attr.json root is not an object");
    return;
  }

  for (auto it_op = json_data.begin(); it_op != json_data.end(); ++it_op) {
    const std::string op_name = it_op.key();
    if (!it_op.value().is_object()) {
      continue;
    }

    for (auto it_attr = it_op.value().begin(); it_attr != it_op.value().end(); ++it_attr) {
      const std::string attr_name = it_attr.key();
      if (!it_attr.value().is_object()) {
        continue;
      }

      ge::JsonFile attr_obj(it_attr.value());

      std::string type;
      if (!attr_obj.Get("type", type)) {
        continue;
      }

      // Get value field as JSON string
      if (!attr_obj.Raw().contains("value")) {
        continue;
      }

      try {
        std::string value_str;
        if (type == "LIST_STRING") {
          // Serialize as [N]value[N]value to match OM1 DavinciModel::GetNodeAttr format
          const auto &value_array = attr_obj.Raw()["value"];
          if (value_array.is_array()) {
            for (const auto &elem : value_array) {
              const std::string s = elem.get<std::string>();
              value_str += "[" + std::to_string(s.size()) + "]" + s;
            }
          }
        } else {
          // For other types, serialize as JSON string
          value_str = attr_obj.Raw()["value"].dump();
        }

        attr_map[op_name][attr_name] = value_str;
      } catch (const std::exception &e) {
        GELOGW("[OM2] Failed to serialize attr value for op[%s] attr[%s]: %s", op_name.c_str(), attr_name.c_str(),
               e.what());
      }
    }
  }
}
}  // namespace

class Om2ModelExecutor::Impl {
 public:
  ge::Status ParseModel(ge::ModelData &model_data, ge::ReadonlyByteBuffer &weight_buf, ge::JsonFile &constants_json,
                        std::vector<KernelBinInfo> &kernel_bin_info) {
    has_model_ = false;
    CloseMemFd(run_model_info_.so_fd);
    run_model_info_ = RunModelInfo();
    model_meta_info_ = ModelMetaInfo();
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
        const ge::JsonFile model_meta_json(buff_data.get(), buff_size);
        GE_ASSERT_TRUE(model_meta_json.IsValid());
        GE_ASSERT_SUCCESS(ParseModelMetaInfo(model_meta_json));
        has_model_ = true;
        break;
      }
    }
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_SUCCESS(ParseArchiveFiles(archive, file_names, weight_buf, constants_json, kernel_bin_info));
    GE_ASSERT_TRUE(!run_model_info_.so_file.empty(), "[OM2] Om2 compiled so not found, need to check .om2 file.");
    return ge::SUCCESS;
  }

 private:
  ge::Status ParseModelMetaInfo(const ge::JsonFile &model_meta_json) {
    GE_ASSERT_TRUE(model_meta_json.IsValid());
    GE_ASSERT_SUCCESS(GetModelJsonValue("name", run_model_info_.model_name, model_meta_json));
    if (!model_meta_json.Get("root_graph_name", run_model_info_.root_graph_name)) {
      run_model_info_.root_graph_name = run_model_info_.model_name;
    }
    GE_ASSERT_SUCCESS(GetModelJsonValue("work_size", model_meta_info_.work_size, model_meta_json));
    GE_ASSERT_SUCCESS(GetModelJsonValue("dynamic_batch_info", model_meta_info_.dynamic_batch_info, model_meta_json));
    GE_ASSERT_SUCCESS(GetModelJsonValue("dynamic_type", model_meta_info_.dynamic_type, model_meta_json));
    GE_ASSERT_SUCCESS(
        GetModelJsonValue("dynamic_output_shape", model_meta_info_.dynamic_output_shape, model_meta_json));
    GE_ASSERT_SUCCESS(
        GetModelJsonValue("user_designate_shape_order", model_meta_info_.user_designate_shape_order, model_meta_json));
    ge::JsonFile::json inputs;
    GE_ASSERT_TRUE(model_meta_json.Get("inputs", inputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(inputs, model_meta_info_.input_desc));
    GE_ASSERT_SUCCESS(SetTensorDesc(inputs, model_meta_info_.input_desc_v2, true));
    // Parse origin_input_dims from each input (original shape with -1 for dynamic axes)
    for (const auto &input_item : inputs) {
      ge::JsonFile input_obj{input_item};
      std::vector<int64_t> origin_dims;
      (void)input_obj.Get("origin_input_dims", origin_dims);  // 没有字段时为空vector，保持索引对齐
      model_meta_info_.origin_input_dims.push_back(std::move(origin_dims));
    }
    ge::JsonFile::json outputs;
    GE_ASSERT_TRUE(model_meta_json.Get("outputs", outputs));
    GE_ASSERT_SUCCESS(SetTensorDesc(outputs, model_meta_info_.output_desc));
    // Output desc currently uses the same schema for both old and new model desc views.
    model_meta_info_.output_desc_v2 = model_meta_info_.output_desc;
    return ge::SUCCESS;
  }

  ge::Status ParseArchiveFiles(ge::RAIIZipArchive &archive, const std::vector<std::string> &file_names,
                               ge::ReadonlyByteBuffer &weight_buf, ge::JsonFile &constants_json,
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
        constants_json = ge::JsonFile(buff_data.get(), buff_size);
        GE_ASSERT_TRUE(constants_json.IsValid());
        continue;
      }
      if (IsFileNameEndsWith(file_name, "op_attr.json")) {
        auto buff_data = archive.ExtractToMem(file_name, buff_size);
        if (buff_data == nullptr || buff_size == 0U) {
          GELOGW("[OM2] op_attr.json not found, using empty json content.");
          run_model_info_.op_attr_json = ge::JsonFile(ge::JsonFile::json::object());
        } else {
          run_model_info_.op_attr_json = ge::JsonFile(buff_data.get(), buff_size);
          if (!run_model_info_.op_attr_json.IsValid()) {
            GELOGW("[OM2] op_attr.json is not valid, using empty json content.");
            run_model_info_.op_attr_json = ge::JsonFile(ge::JsonFile::json::object());
          }
        }
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
        GE_ASSERT_SUCCESS(
            CreateSoMemFd(file_name, buff_data.get(), buff_size, run_model_info_.so_file, run_model_info_.so_fd));
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
    run_model_info_.load_func = reinterpret_cast<LoadFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelLoad"));
    GE_ASSERT_NOTNULL(run_model_info_.load_func);
    run_model_info_.destroy_func = reinterpret_cast<DestroyFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelDestroy"));
    GE_ASSERT_NOTNULL(run_model_info_.destroy_func);
    run_model_info_.run_func = reinterpret_cast<RunFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelRun"));
    GE_ASSERT_NOTNULL(run_model_info_.run_func);
    run_model_info_.run_async_func =
        reinterpret_cast<RunAsyncFunc>(mmDlsym(run_model_info_.so_handle, "Om2ModelRunAsync"));
    GE_ASSERT_NOTNULL(run_model_info_.run_async_func);
    return ge::SUCCESS;
  }

  ge::Status CreateModel(ge::ModelData &model_data, ge::ReadonlyByteBuffer &weight_buf,
                         std::vector<KernelBinInfo> &kernel_bin_info, const ge::JsonFile &constants_json,
                         const Om2ModelLoadArg &load_arg, uint64_t session_id, std::vector<void *> &constants) {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_TRUE(load_arg.device_id >= 0, "[OM2][Check] Invalid device id.");
    device_id_ = load_arg.device_id;
    std::vector<const char *> bin_files(kernel_bin_info.size());
    std::vector<const void *> bin_data(kernel_bin_info.size());
    std::vector<size_t> bin_sizes(kernel_bin_info.size());
    void *work_ptr = nullptr;
    for (auto i = 0U; i < kernel_bin_info.size(); ++i) {
      bin_files[i] = kernel_bin_info[i].file.c_str();
      bin_data[i] = kernel_bin_info[i].data.get();
      bin_sizes[i] = kernel_bin_info[i].data_size;
    }
    session_id_ = session_id;
    GE_ASSERT_SUCCESS(PrepareWorkPtr(load_arg, work_ptr));
    GE_ASSERT_SUCCESS(PrepareConstants(model_data, weight_buf, constants_json, load_arg, constants));
    GE_ASSERT_SUCCESS(run_model_info_.create_func(
        &run_model_info_.model_handle, &run_model_info_.rt_model_handle, bin_files.data(), bin_data.data(),
        bin_sizes.data(), static_cast<int>(bin_data.size()), constants.empty() ? nullptr : constants.data(), work_ptr,
        &session_id_, load_arg.model_id, dump_manager_.get()));
    return ge::GRAPH_SUCCESS;
  }

  ge::Status CreateAndLoadModel(ge::ModelData &model_data, ge::ReadonlyByteBuffer &weight_buf,
                                std::vector<KernelBinInfo> &kernel_bin_info, const ge::JsonFile &constants_json,
                                const Om2ModelLoadArg &load_arg, uint64_t session_id) {
    // The generated model consumes this pointer array during create/load, so keep it alive until LoadModel returns.
    std::vector<void *> constants;
    GE_ASSERT_SUCCESS(
        CreateModel(model_data, weight_buf, kernel_bin_info, constants_json, load_arg, session_id, constants));
    GE_ASSERT_SUCCESS(InitModelDumpInfo(load_arg));
    ReportModelLoadBegin();
    GE_ASSERT_SUCCESS(LoadModel());
    ReportModelLoadEnd();
    GE_ASSERT_SUCCESS(DispatchDumpInfo());
    weight_buf.reset(nullptr);
    kernel_bin_info.clear();
    return ge::GRAPH_SUCCESS;
  }

  ge::Status CreateDumpManager(const Om2ModelLoadArg &load_arg) {
    model_id_ = load_arg.model_id;
    dump_manager_ =
        std::unique_ptr<ge::dump::ModelDumpManager>(new (std::nothrow) ge::dump::ModelDumpManager(load_arg.model_id));
    GE_ASSERT_TRUE(dump_manager_ != nullptr);
    dump_manager_->SetClearDfxCacheFlagAfterLoad(load_arg.need_clear_dfx_cache);
    return ge::SUCCESS;
  }

  ge::Status InitModelDumpInfo(const Om2ModelLoadArg &load_arg) {
    GE_ASSERT_NOTNULL(run_model_info_.rt_model_handle);
    GE_ASSERT_TRUE(dump_manager_ != nullptr);
    ge::dump::ModelDumpInfo model_dump_info{};
    model_dump_info.model_id = load_arg.model_id;
    model_dump_info.model_name = run_model_info_.model_name.c_str();
    model_dump_info.root_graph_name = run_model_info_.root_graph_name.c_str();
    model_dump_info.device_id = static_cast<uint32_t>(load_arg.device_id);
    model_dump_info.rt_model_handle = run_model_info_.rt_model_handle;
    model_dump_info.step_id_addr = 0U;
    model_dump_info.loop_cond_addr = 0U;
    model_dump_info.iterations_per_loop_addr = 0U;
    GELOGI(
        "[OM2][Dump] Set model dump info: model_id=%u, model_name=%s, root_graph_name=%s, device_id=%u, "
        "rt_model_handle=%p, step_id_addr=%" PRIu64 ", loop_cond_addr=%" PRIu64 ", iterations_per_loop_addr=%" PRIu64
        ".",
        model_dump_info.model_id, model_dump_info.model_name, model_dump_info.root_graph_name,
        model_dump_info.device_id, model_dump_info.rt_model_handle, model_dump_info.step_id_addr,
        model_dump_info.loop_cond_addr, model_dump_info.iterations_per_loop_addr);
    GE_ASSERT_SUCCESS(dump_manager_->SetModelDumpInfo(model_dump_info));
    GELOGI("[OM2][Dump] Set model dump info success, model_id=%u.", model_dump_info.model_id);
    return ge::SUCCESS;
  }

  void ReportModelLoadBegin() const {
    if (dump_manager_ == nullptr) {
      return;
    }
    const ge::Status ret = dump_manager_->ReportModelLoadBegin();
    if (ret != ge::SUCCESS) {
      GELOGW("[OM2][Profiling] Report model load begin failed, model_id=%u, ret=%u.", model_id_, ret);
    }
  }

  void ReportModelLoadEnd() const {
    if (dump_manager_ == nullptr) {
      return;
    }
    const ge::Status ret = dump_manager_->ReportModelLoadEnd();
    if (ret != ge::SUCCESS) {
      GELOGW("[OM2][Profiling] Report model load end failed, model_id=%u, ret=%u.", model_id_, ret);
    }
  }

  ge::Status LoadModel() {
    GE_ASSERT_NOTNULL(run_model_info_.load_func);
    GE_ASSERT_NOTNULL(run_model_info_.model_handle);
    GE_ASSERT_SUCCESS(run_model_info_.load_func(&run_model_info_.model_handle));
    return ge::SUCCESS;
  }

  ge::Status DispatchDumpInfo() {
    GE_ASSERT_TRUE(dump_manager_ != nullptr);
    GELOGI("[OM2][Dump] Dispatch dump info begin, model_id=%u, model_name=%s, root_graph_name=%s.", model_id_,
           run_model_info_.model_name.c_str(), run_model_info_.root_graph_name.c_str());
    GE_ASSERT_SUCCESS(dump_manager_->DispatchDumpInfo());
    GELOGI("[OM2][Dump] Dispatch dump info success, model_id=%u, model_name=%s, root_graph_name=%s.", model_id_,
           run_model_info_.model_name.c_str(), run_model_info_.root_graph_name.c_str());
    return ge::SUCCESS;
  }

  ge::Status Run(std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) {
    GE_ASSERT_TRUE(has_model_);
    GE_ASSERT_NOTNULL(run_model_info_.run_func);
    GE_ASSERT_NOTNULL(run_model_info_.model_handle);
    // 从 OM2Context 获取 timeout 值并传递给 run_func
    int32_t timeout = GetOm2ThreadLocalContext().StreamSyncTimeout();
    GE_ASSERT_SUCCESS(run_model_info_.run_func(&run_model_info_.model_handle, inputs.size(),
                                               reinterpret_cast<void **>(inputs.data()), outputs.size(),
                                               reinterpret_cast<void **>(outputs.data()), timeout));
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
    dynamic_batch_info = model_meta_info_.dynamic_batch_info;
    dynamic_type = model_meta_info_.dynamic_type;
    return ge::SUCCESS;
  }

  ge::Status GetModelAttrs(std::vector<std::string> &dynamic_output_shape) const {
    GE_ASSERT_TRUE(has_model_);
    dynamic_output_shape = model_meta_info_.dynamic_output_shape;
    return ge::SUCCESS;
  }

  ge::Status GetModelDescInfo(const std::vector<ge::Om2TensorDesc> *&input_desc,
                              const std::vector<ge::Om2TensorDesc> *&output_desc, const bool new_model_desc) const {
    GE_ASSERT_TRUE(has_model_);
    if (new_model_desc) {
      input_desc = &model_meta_info_.input_desc_v2;
      output_desc = &model_meta_info_.output_desc_v2;
    } else {
      input_desc = &model_meta_info_.input_desc;
      output_desc = &model_meta_info_.output_desc;
    }
    return ge::SUCCESS;
  }

  ge::Status GetUserDesignateShapeOrder(std::vector<std::string> &user_designate_shape_order) const {
    GE_ASSERT_TRUE(has_model_);
    user_designate_shape_order = model_meta_info_.user_designate_shape_order;
    return ge::SUCCESS;
  }

  const std::vector<std::vector<int64_t>> &GetOriginInputDims() const {
    return model_meta_info_.origin_input_dims;
  }

  ge::Status GetOpAttr(std::map<std::string, std::map<std::string, std::string>> &op_attr_map) const {
    GE_ASSERT_TRUE(has_model_);
    op_attr_map.clear();

    if (!run_model_info_.op_attr_json.IsValid()) {
      return ge::SUCCESS;  // Empty map for invalid/missing JSON
    }

    ParseOpAttrJsonToMapInternal(run_model_info_.op_attr_json, op_attr_map);
    return ge::SUCCESS;
  }

  ge::Status GetOpDescInfo(const uint32_t device_id, const uint32_t stream_id, const uint32_t task_id,
                           ge::OpDescInfo &op_desc_info) const {
    GE_ASSERT_TRUE(has_model_);
    if (device_id_ != static_cast<int32_t>(device_id)) {
      GELOGD("[OM2][Get][OpDescInfo] Device id not match, input=%u, model=%d.", device_id, device_id_);
      return ge::FAILED;
    }
    GE_ASSERT_NOTNULL(dump_manager_);
    return dump_manager_->GetOpDescInfo(ge::OpDescInfoId(task_id, stream_id, static_cast<int32_t>(device_id)),
                                        op_desc_info)
               ? ge::SUCCESS
               : ge::FAILED;
  }

  ge::Status SetDynamicSize(const std::vector<uint64_t> &batch_num, const int32_t dynamic_type) {
    GE_ASSERT_TRUE(has_model_);

    int32_t model_dynamic_type = static_cast<int32_t>(ge::FIXED);
    std::vector<std::vector<int64_t>> dynamic_batch_info;
    ge::Status ret = GetDynamicBatchInfo(dynamic_batch_info, model_dynamic_type);
    if (ret != ge::SUCCESS) {
      GELOGE(ge::FAILED, "[OM2][SetDynamicSize] GetDynamicBatchInfo failed, ret=%u.", ret);
      return ge::FAILED;
    }

    if (dynamic_type != model_dynamic_type) {
      GELOGE(ge::FAILED, "[OM2][SetDynamicSize] dynamic_type mismatch: requested %d, model compiled with %d.",
             dynamic_type, model_dynamic_type);
      return ge::FAILED;
    }

    std::vector<int64_t> requested_gear;
    requested_gear.reserve(batch_num.size());
    for (const auto v : batch_num) {
      requested_gear.push_back(static_cast<int64_t>(v));
    }

    bool gear_found = false;
    for (const auto &gear : dynamic_batch_info) {
      if (gear == requested_gear) {
        gear_found = true;
        break;
      }
    }

    if (!gear_found) {
      GELOGE(ge::FAILED, "[OM2][SetDynamicSize] Requested gear not found in dynamic_batch_info.");
      return ge::FAILED;
    }

    cur_batch_size_ = batch_num;
    dynamic_type_ = dynamic_type;
    GELOGI("[OM2][SetDynamicSize] Set dynamic size success, type=%d, size=%zu.", dynamic_type_, cur_batch_size_.size());
    return ge::SUCCESS;
  }

  ge::Status GetCurrentShape(std::vector<int64_t> &batch_info, int32_t &dynamic_type) const {
    GE_ASSERT_TRUE(has_model_);
    batch_info.clear();
    if (cur_batch_size_.empty()) {
      GELOGD("[OM2][GetCurrentShape] User has not set dynamic size.");
      dynamic_type = static_cast<int32_t>(ge::FIXED);
      return ge::SUCCESS;
    }
    for (const auto v : cur_batch_size_) {
      batch_info.emplace_back(static_cast<int64_t>(v));
    }
    dynamic_type = dynamic_type_;
    return ge::SUCCESS;
  }

  void Cleanup() {
    if (dump_manager_ != nullptr) {
      dump_manager_.reset();
    }
    if (run_model_info_.destroy_func != nullptr && run_model_info_.model_handle != nullptr) {
      const auto destroy_ret = run_model_info_.destroy_func(&run_model_info_.model_handle);
      if (destroy_ret != ge::GRAPH_SUCCESS) {
        GELOGI("[OM2] Resource release issue for so file: %s", run_model_info_.so_file.c_str());
      }
    } else {
      GELOGI("[OM2] Destroy func not found or model not created, so file: %s", run_model_info_.so_file.c_str());
    }
    if (run_model_info_.so_handle != nullptr) {
      if (mmDlclose(run_model_info_.so_handle) != 0) {
        const char_t *error = mmDlerror();
        error = (error == nullptr) ? "" : error;
        GELOGI("[OM2][Dlclose] path = %s, error = %s", run_model_info_.so_file.c_str(), error);
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
    const size_t required_work_size = model_meta_info_.work_size;
    if (load_arg.work_ptr != nullptr) {
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
                              const ge::JsonFile &constants_json, const Om2ModelLoadArg &load_arg,
                              std::vector<void *> &constants) {
    std::vector<Om2ConstItem> const_items;
    size_t internal_weight_size = 0U;
    GE_ASSERT_SUCCESS(ParseConstItems(constants_json, const_items, internal_weight_size));
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    ClassifiedConstItems classified_items;
    GE_ASSERT_SUCCESS(ClassifyConstItems(const_items, classified_items));
    constants.resize(classified_items.max_index + 1U, nullptr);
    std::map<std::string, ge::FileConstantMem> user_file_const_mems;
    GE_ASSERT_SUCCESS(BuildUserFileConstMemMap(load_arg.file_constant_mems, user_file_const_mems));
    GE_ASSERT_SUCCESS(
        PrepareInternalConsts(weight_buf, load_arg, classified_items.internal_consts, internal_weight_size, constants));
    GE_ASSERT_SUCCESS(
        PrepareCombinedConsts(model_data, user_file_const_mems, classified_items.combined_consts, constants));
    GE_ASSERT_SUCCESS(
        PrepareIndividualConsts(model_data, user_file_const_mems, classified_items.individual_consts, constants));
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
    FileConstContext file_const_ctx;
    GE_ASSERT_SUCCESS(BuildFileConstContext(model_data, user_file_const_mems, file_const_ctx));
    GE_ASSERT_SUCCESS(gert::PrepareCombinedConsts(const_items, file_const_ctx, constants));
    return ge::SUCCESS;
  }

  ge::Status PrepareIndividualConsts(const ge::ModelData &model_data,
                                     const std::map<std::string, ge::FileConstantMem> &user_file_const_mems,
                                     const std::vector<Om2ConstItem> &const_items, std::vector<void *> &constants) {
    if (const_items.empty()) {
      return ge::SUCCESS;
    }
    FileConstContext file_const_ctx;
    GE_ASSERT_SUCCESS(BuildFileConstContext(model_data, user_file_const_mems, file_const_ctx));
    GE_ASSERT_SUCCESS(gert::PrepareIndividualConsts(const_items, file_const_ctx, device_id_, constants));
    return ge::SUCCESS;
  }

  ge::Status BuildFileConstContext(const ge::ModelData &model_data,
                                   const std::map<std::string, ge::FileConstantMem> &user_file_const_mems,
                                   FileConstContext &file_const_ctx) {
    std::string weight_dir;
    GE_ASSERT_SUCCESS(ResolveFileConstWeightDir(model_data, weight_dir));
    file_const_ctx.weight_dir = weight_dir;
    file_const_ctx.user_file_const_mems = &user_file_const_mems;
    file_const_ctx.owned_buffers = &owned_buffers_;
    file_const_ctx.session_id = session_id_;
    file_const_ctx.device_id = device_id_;
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
  ModelMetaInfo model_meta_info_;
  std::unique_ptr<ge::dump::ModelDumpManager> dump_manager_;
  std::vector<void *> owned_buffers_;
  uint32_t model_id_ = 0U;
  int32_t device_id_ = -1;
  uint64_t session_id_ = 0U;
  bool has_model_ = false;
  // 当前档位维度值，由 SetDynamicSize 写入，GetCurrentShape 读取
  std::vector<uint64_t> cur_batch_size_;
  int32_t dynamic_type_ = 0;  // 0=FIXED
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
  ge::JsonFile constants_json;
  std::vector<KernelBinInfo> kernel_bin_info;
  GE_CHK_STATUS_RET(impl_->ParseModel(model_data, weight_buf, constants_json, kernel_bin_info),
                    "[OM2][Load] Parse model failed.");
  GE_ASSERT_SUCCESS(impl_->LoadSharedObject());
  GE_ASSERT_SUCCESS(impl_->ResolveSymbols());
  GE_ASSERT_SUCCESS(impl_->CreateDumpManager(load_arg));
  GE_ASSERT_SUCCESS(
      impl_->CreateAndLoadModel(model_data, weight_buf, kernel_bin_info, constants_json, load_arg, session_id));
  return ge::SUCCESS;
}

ge::Status Om2ModelExecutor::Run(std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) const {
  return impl_->Run(inputs, outputs);
}

ge::Status Om2ModelExecutor::RunAsync(void *const stream, std::vector<gert::Tensor *> &inputs,
                                      std::vector<gert::Tensor *> &outputs) const {
  return impl_->RunAsync(stream, inputs, outputs);
}

ge::Status Om2ModelExecutor::GetModelDescInfo(const std::vector<ge::Om2TensorDesc> *&input_desc,
                                              const std::vector<ge::Om2TensorDesc> *&output_desc,
                                              bool new_model_desc) const {
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

ge::Status Om2ModelExecutor::SetDynamicSize(const std::vector<uint64_t> &batch_num, int32_t dynamic_type) {
  return impl_->SetDynamicSize(batch_num, dynamic_type);
}

ge::Status Om2ModelExecutor::GetCurrentShape(std::vector<int64_t> &batch_info, int32_t &dynamic_type) const {
  return impl_->GetCurrentShape(batch_info, dynamic_type);
}

const std::vector<std::vector<int64_t>> &Om2ModelExecutor::GetOriginInputDims() const {
  return impl_->GetOriginInputDims();
}

ge::Status Om2ModelExecutor::GetOpAttr(std::map<std::string, std::map<std::string, std::string>> &op_attr_map) const {
  return impl_->GetOpAttr(op_attr_map);
}

ge::Status Om2ModelExecutor::GetOpDescInfo(uint32_t device_id, uint32_t stream_id, uint32_t task_id,
                                           ge::OpDescInfo &op_desc_info) const {
  return impl_->GetOpDescInfo(device_id, stream_id, task_id, op_desc_info);
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

ge::Status GetOm2ModelMetadata(const std::string &model_path, std::vector<ge::Om2TensorDesc> &input_desc,
                               std::vector<ge::Om2TensorDesc> &input_desc_v2,
                               std::vector<ge::Om2TensorDesc> &output_desc,
                               std::vector<ge::Om2TensorDesc> &output_desc_v2) {
  ge::ModelData model_data;
  GE_CHK_STATUS_RET(LoadOm2DataFromFile(model_path, model_data), "[OM2][Query] Load model data from file failed.");
  std::shared_ptr<void> data_guarder(model_data.model_data, [](const void *const p) {
    if (p != nullptr) {
      delete[] static_cast<const uint8_t *>(p);
    }
  });
  ge::RAIIZipArchive archive(static_cast<uint8_t *>(model_data.model_data), model_data.model_len);
  GE_ASSERT_TRUE(archive.IsGood());
  GE_ASSERT_SUCCESS(GetOm2ModelMetadataFromArchive(archive, input_desc, input_desc_v2, output_desc, output_desc_v2));
  return ge::SUCCESS;
}

ge::Status GetOm2ModelMetadata(const void *model_data, size_t model_size, std::vector<ge::Om2TensorDesc> &input_desc,
                               std::vector<ge::Om2TensorDesc> &input_desc_v2,
                               std::vector<ge::Om2TensorDesc> &output_desc,
                               std::vector<ge::Om2TensorDesc> &output_desc_v2) {
  ge::RAIIZipArchive archive(static_cast<const uint8_t *>(model_data), model_size);
  GE_ASSERT_TRUE(archive.IsGood());
  GE_ASSERT_SUCCESS(GetOm2ModelMetadataFromArchive(archive, input_desc, input_desc_v2, output_desc, output_desc_v2));
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
    REPORT_PREDEFINED_ERR_MSG("E13000", std::vector<const char *>({"path", "errmsg"}),
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
