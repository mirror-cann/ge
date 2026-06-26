/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/ge_common/string_util.h"
#include "framework/common/helper/om2_package_helper.h"
#include "framework/common/helper/model_save_helper_factory.h"
#include "common/file_constant_utils/file_constant_utils.h"
#include "common/ge_common/ge_types.h"
#include "common/helper/om2/zip_archive_writer.h"
#include "common/helper/om2/om2_package_contants.h"
#include "common/helper/om2/json_file.h"
#include "common/om2/codegen/om2_codegen.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "framework/omg/omg_inner_types.h"
#include "graph/debug/ge_attr_define.h"
#include "graph_metadef/common/plugin/plugin_manager.h"
#include "graph/utils/type_utils.h"
#include "graph/utils/tensor_utils.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "graph/model.h"
#include "proto/ge_ir.pb.h"
#include "proto/onnx/ge_onnx.pb.h"
#include "graph/utils/ge_ir_utils.h"
#include <google/protobuf/text_format.h>
#include <iomanip>
#include <sstream>

namespace ge {
namespace {
constexpr auto kAttrKernelName = "_kernelname";
const std::string kOm2ConstantsConfigSuffix = "_constants_config.json";
const std::string kOm2ExternalWeightDirName = "weight";

struct ModelIoNodes {
  std::map<uint32_t, OpDescPtr> input_ops;
  std::vector<OpDescPtr> output_ops;
  std::vector<OpDescPtr> case_ops;
};

struct ModelMetaExtraInfo {
  JsonFile::json dynamic_output_shape = JsonFile::json::array();
  JsonFile::json dynamic_batch_info = JsonFile::json::array();
  JsonFile::json user_designate_shape_order = JsonFile::json::array();
  int32_t dynamic_type = 0;
};

Status GetDynamicBatchInfo(const OpDescPtr &op_desc, JsonFile::json &batch_info,
                           JsonFile::json &user_designate_shape_order, int32_t &dynamic_type) {
  uint32_t batch_num = 0U;
  if (!AttrUtils::GetInt(op_desc, ATTR_NAME_BATCH_NUM, batch_num)) {
    GELOGI("Not multi-batch Node: %s", op_desc->GetName().c_str());
    return SUCCESS;
  }
  batch_info.clear();

  (void)AttrUtils::GetInt(op_desc, ATTR_DYNAMIC_TYPE, dynamic_type);
  std::vector<std::string> user_designate_shape_order_vec;
  (void)AttrUtils::GetListStr(op_desc, ATTR_USER_DESIGNEATE_SHAPE_ORDER, user_designate_shape_order_vec);
  for (const auto &s : user_designate_shape_order_vec) {
    user_designate_shape_order.push_back(s);
  }
  for (uint32_t i = 0U; i < batch_num; ++i) {
    std::vector<int64_t> batch_shape;
    const std::string attr_name = ATTR_NAME_PRED_VALUE + "_" + std::to_string(i);
    if (!AttrUtils::GetListInt(op_desc, attr_name, batch_shape)) {
      REPORT_INNER_ERR_MSG("E19999", "Get Attr:%s from op:%s(%s) fail", attr_name.c_str(), op_desc->GetName().c_str(),
                           op_desc->GetType().c_str());
      GELOGE(FAILED, "[Get][Attr] %s from op:%s(%s) fail", attr_name.c_str(), op_desc->GetName().c_str(),
             op_desc->GetType().c_str());
      batch_info.clear();
      return FAILED;
    }
    batch_info.push_back(batch_shape);
  }
  return SUCCESS;
}

Status FillTensorInfo(JsonFile &tensor_info, const ConstGeTensorDescPtr &tensor_desc, const int64_t tensor_size) {
  (void)tensor_info.Set("shape", tensor_desc->GetShape().GetDims())
      .Set("data_type", TypeUtils::DataTypeToSerialString(tensor_desc->GetDataType()))
      .Set("format", TypeUtils::FormatToSerialString(tensor_desc->GetFormat()))
      .Set("size", tensor_size);
  std::vector<std::pair<int64_t, int64_t>> range;
  if (tensor_desc->GetShapeRange(range) == SUCCESS) {
    (void)tensor_info.Set("shape_range", range);
  }
  return SUCCESS;
}

bool EndsWith(const std::string &str, const std::string &suffix) {
  return (str.size() >= suffix.size()) && (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

std::string StripOm2ArchiveRoot(const std::string &entry_name) {
  const auto pos = entry_name.find('/');
  return (pos == std::string::npos) ? entry_name : entry_name.substr(pos + 1U);
}

bool IsOm2ConstantsConfigEntry(const std::string &entry_name) {
  return (entry_name.find(OM2_CONSTANTS_DIR) == 0U) && EndsWith(entry_name, kOm2ConstantsConfigSuffix);
}

bool ShouldCompressRepackedOm2Entry(const std::string &entry_name) {
  const std::string model_dir_prefix = "data/model_";
  const std::string runtime_dir = "/runtime/";
  if (entry_name.find(model_dir_prefix) != 0U) {
    return false;
  }
  const auto model_index_start = model_dir_prefix.length();
  const auto runtime_pos = entry_name.find(runtime_dir, model_index_start);
  return (runtime_pos != std::string::npos) && (runtime_pos > model_index_start);
}

std::string MakeOm2ExternalWeightPath(const std::string &output_file_name, const std::string &file_name) {
  std::string path = output_file_name;
  const char_t *const om_dir = mmDirName(&path[0]);
  if (om_dir == nullptr) {
    return "";
  }
  return std::string(om_dir) + "/" + kOm2ExternalWeightDirName + "/" + file_name;
}

Status RewriteOm2ConstantsConfig(const std::string &output_file_name, JsonFile &constants_json,
                                 std::map<std::string, std::string> &old_file_to_new_file, bool &changed) {
  JsonFile::json consts_json;
  if (!constants_json.Get("consts", consts_json) || !consts_json.is_object()) {
    return SUCCESS;
  }
  for (auto &const_item : consts_json.items()) {
    auto &const_info = const_item.value();
    if (!const_info.is_object()) {
      continue;
    }
    JsonFile const_info_json(const_info);
    std::string type;
    if (const_info_json.Get("type", type) && type == "INTERNAL") {
      continue;
    }
    std::string old_file_path;
    if (!const_info_json.Get("file_path", old_file_path) || old_file_path.empty()) {
      continue;
    }
    std::string file_name;
    if (!const_info_json.Get("file_name", file_name) || file_name.empty()) {
      file_name = StringUtils::GetFileName(old_file_path);
    }
    GE_ASSERT_TRUE(!file_name.empty(), "[OM2] External weight file name is empty, file_path=%s", old_file_path.c_str());
    const std::string new_file_path = MakeOm2ExternalWeightPath(output_file_name, file_name);
    GE_ASSERT_TRUE(!new_file_path.empty(), "[OM2] Failed to make external weight path, output=%s",
                   output_file_name.c_str());
    const_info["file_name"] = file_name;
    (void)const_info.erase("file_path");
    old_file_to_new_file[old_file_path] = new_file_path;
    changed = true;
  }
  if (changed) {
    (void)constants_json.Set("consts", consts_json);
  }
  return SUCCESS;
}

Status CollectOm2ExternalWeightRelocation(const std::string &output_file_name, const SimpleZipArchiveReader &archive,
                                          const std::vector<std::string> &archive_entries,
                                          std::map<std::string, std::string> &rewritten_configs,
                                          std::map<std::string, std::string> &old_file_to_new_file) {
  for (const auto &entry_name : archive_entries) {
    const std::string relative_entry_name = StripOm2ArchiveRoot(entry_name);
    if (!IsOm2ConstantsConfigEntry(relative_entry_name)) {
      continue;
    }
    size_t buffer_size = 0U;
    const auto buffer = archive.ExtractToMem(entry_name, buffer_size);
    GE_ASSERT_NOTNULL(buffer, "[OM2] Failed to extract constants config entry %s", entry_name.c_str());
    const JsonFile const_json_readonly(reinterpret_cast<const uint8_t *>(buffer.get()), buffer_size);
    GE_ASSERT_TRUE(const_json_readonly.IsValid(), "[OM2] Invalid constants config entry %s", entry_name.c_str());
    JsonFile const_json(const_json_readonly.Raw());
    bool changed = false;
    GE_ASSERT_SUCCESS(RewriteOm2ConstantsConfig(output_file_name, const_json, old_file_to_new_file, changed));
    if (changed) {
      rewritten_configs[entry_name] = const_json.Dump();
    }
  }
  return SUCCESS;
}

Status RepackOm2Model(const std::string &output_file_name, const SimpleZipArchiveReader &archive,
                      const std::vector<std::string> &archive_entries,
                      const std::map<std::string, std::string> &rewritten_configs, ModelBufferData &relocated_model) {
  auto zip_writer = MakeShared<ZipArchiveWriter>(output_file_name);
  GE_ASSERT_NOTNULL(zip_writer);
  GE_ASSERT_TRUE(zip_writer->IsMemFileOpened());
  for (const auto &entry_name : archive_entries) {
    const std::string relative_entry_name = StripOm2ArchiveRoot(entry_name);
    const auto rewritten_config = rewritten_configs.find(entry_name);
    if (rewritten_config != rewritten_configs.end()) {
      GE_ASSERT_TRUE(zip_writer->WriteBytes(relative_entry_name, rewritten_config->second.data(),
                                            rewritten_config->second.size(),
                                            ShouldCompressRepackedOm2Entry(relative_entry_name)));
      continue;
    }
    size_t buffer_size = 0U;
    const auto buffer = archive.ExtractToMem(entry_name, buffer_size);
    GE_ASSERT_NOTNULL(buffer, "[OM2] Failed to extract archive entry %s", entry_name.c_str());
    GE_ASSERT_TRUE(buffer_size > 0U, "[OM2] Empty archive entry %s is invalid", entry_name.c_str());
    GE_ASSERT_TRUE(zip_writer->WriteBytes(relative_entry_name, buffer.get(), buffer_size,
                                          ShouldCompressRepackedOm2Entry(relative_entry_name)));
  }
  GE_ASSERT_TRUE(zip_writer->SaveModelData(relocated_model, false));
  return SUCCESS;
}

Status CollectModelIoNodes(const ComputeGraphPtr &graph, ModelIoNodes &io_nodes) {
  uint32_t data_index = 0U;
  const std::set<std::string> kDataOpTypes{DATA, REFDATA, AIPPDATA, ANN_DATA};
  for (const auto &node : graph->GetDirectNode()) {
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    if (kDataOpTypes.count(op_desc->GetType()) > 0U) {
      uint32_t tmp_index = data_index++;
      if (AttrUtils::GetInt(op_desc, ATTR_NAME_INDEX, tmp_index)) {
        GELOGD("Get new data index %u, old index is %u", tmp_index, data_index - 1U);
      }
      io_nodes.input_ops[tmp_index] = op_desc;
      GELOGD("Find input node [%s], index [%u]", node->GetNamePtr(), tmp_index);
      continue;
    }
    if (op_desc->GetType() == NETOUTPUT) {
      io_nodes.output_ops.push_back(op_desc);
      GELOGD("Find output node [%s]", node->GetNamePtr());
    }
    if (op_desc->GetType() == CASE) {
      io_nodes.case_ops.push_back(op_desc);
      GELOGD("Find case node [%s]", node->GetNamePtr());
    }
  }
  return SUCCESS;
}

Status BuildInputJsonArray(const std::map<uint32_t, OpDescPtr> &input_ops, JsonFile::json &input_json_array) {
  for (const auto &[index, op_desc] : input_ops) {
    const auto &tensor_desc = op_desc->GetInputDescPtr(0);
    GE_ASSERT_NOTNULL(tensor_desc);
    int64_t input_size = 0;
    const auto output_desc = op_desc->GetOutputDescPtr(0U);
    if ((output_desc != nullptr) && AttrUtils::GetInt(*output_desc, ATTR_NAME_SPECIAL_INPUT_SIZE, input_size) &&
        (input_size > 0)) {
      GELOGI("data[%s] output has special size [%" PRId64 "]", op_desc->GetName().c_str(), input_size);
    } else {
      GE_CHK_STATUS_RET(TensorUtils::GetSize(*tensor_desc, input_size), "[Get][InputSize] failed in op: %s.",
                        op_desc->GetName().c_str());
    }
    JsonFile input_info;
    (void)input_info.Set("name", op_desc->GetName()).Set("index", index);
    GE_ASSERT_SUCCESS(FillTensorInfo(input_info, tensor_desc, input_size));
    std::vector<int64_t> model_input_dims;
    if (op_desc->HasAttr(ATTR_NAME_INPUT_DIMS)) {
      (void)AttrUtils::GetListInt(op_desc, ATTR_NAME_INPUT_DIMS, model_input_dims);
    } else {
      model_input_dims = tensor_desc->GetShape().GetDims();
    }
    (void)input_info.Set("shape_v2", model_input_dims);
    // Serialize original shape: with -1 for dynamic axes (from MultiBatchClonePass), or same as shape for static models
    std::vector<int64_t> origin_input_dims;
    if (op_desc->HasAttr(ATTR_MBATCH_ORIGIN_INPUT_DIMS) &&
        AttrUtils::GetListInt(op_desc, ATTR_MBATCH_ORIGIN_INPUT_DIMS, origin_input_dims)) {
      input_info.Set("origin_input_dims", origin_input_dims);
    } else {
      input_info.Set("origin_input_dims", tensor_desc->GetShape().GetDims());
    }
    input_json_array.push_back(input_info.Raw());
  }
  return SUCCESS;
}

Status CollectDynamicBatchInfo(const std::vector<OpDescPtr> &case_ops, ModelMetaExtraInfo &extra_info) {
  for (const auto &op_desc : case_ops) {
    GE_ASSERT_SUCCESS(GetDynamicBatchInfo(op_desc, extra_info.dynamic_batch_info, extra_info.user_designate_shape_order,
                                          extra_info.dynamic_type));
  }
  return SUCCESS;
}

Status BuildOutputJsonArray(const std::vector<OpDescPtr> &output_ops, const std::vector<std::string> &out_node_name,
                            JsonFile::json &output_json_array, ModelMetaExtraInfo &extra_info) {
  for (const auto &op_desc : output_ops) {
    const auto out_size = op_desc->GetInputsSize();
    const auto src_name = op_desc->GetSrcName();
    const auto src_index = op_desc->GetSrcIndex();
    GE_ASSERT_TRUE(src_name.size() >= out_size && src_index.size() >= out_size);
    for (size_t i = 0UL; i < out_size; ++i) {
      JsonFile output_info;
      std::string output_name;
      if (out_size == out_node_name.size()) {
        const bool contains_colon = out_node_name[i].find(':') != std::string::npos;
        output_name = contains_colon ? out_node_name[i] : (out_node_name[i] + ":" + std::to_string(src_index[i]));
      } else {
        output_name =
            std::string("output_") + std::to_string(i) + "_" + src_name[i] + "_" + std::to_string(src_index[i]);
      }
      const auto &tensor_desc = op_desc->GetInputDescPtr(static_cast<uint32_t>(i));
      GE_ASSERT_NOTNULL(tensor_desc);
      int64_t tensor_size = 0;
      if (AttrUtils::GetInt(tensor_desc, ATTR_NAME_SPECIAL_OUTPUT_SIZE, tensor_size) && (tensor_size > 0)) {
        GELOGI("netoutput[%s] [%zu]th input has special size [%" PRId64 "]", op_desc->GetName().c_str(), i,
               tensor_size);
      } else {
        (void)TensorUtils::GetTensorSizeInBytes(*tensor_desc, tensor_size);
      }
      (void)output_info.Set("name", output_name).Set("index", i);
      GE_ASSERT_SUCCESS(FillTensorInfo(output_info, tensor_desc, tensor_size));
      output_json_array.push_back(output_info.Raw());
    }
    std::vector<std::string> shape_info;
    if (AttrUtils::GetListStr(op_desc, ATTR_NAME_DYNAMIC_OUTPUT_DIMS, shape_info)) {
      for (const auto &s : shape_info) {
        extra_info.dynamic_output_shape.push_back(s);
      }
    }
  }
  return SUCCESS;
}

void FillModelMetaInfo(const GeModelPtr &ge_model, const JsonFile::json &input_json_array,
                       const JsonFile::json &output_json_array, const ModelMetaExtraInfo &extra_info,
                       const std::string &root_graph_name, JsonFile &model_meta_info) {
  int64_t work_size = 0;
  (void)AttrUtils::GetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, work_size);
  (void)model_meta_info.Set("inputs", input_json_array);
  (void)model_meta_info.Set("outputs", output_json_array);
  (void)model_meta_info.Set("dynamic_output_shape", extra_info.dynamic_output_shape);
  (void)model_meta_info.Set("dynamic_batch_info", extra_info.dynamic_batch_info);
  (void)model_meta_info.Set("user_designate_shape_order", extra_info.user_designate_shape_order);
  (void)model_meta_info.Set("dynamic_type", extra_info.dynamic_type);
  (void)model_meta_info.Set("work_size", work_size);
  (void)model_meta_info.Set("name", ge_model->GetName());
  (void)model_meta_info.Set("root_graph_name", root_graph_name);
}

std::string GetRootGraphName(const GeModelPtr &ge_model) {
  if (ge_model == nullptr) {
    return "";
  }
  auto graph = ge_model->GetGraph();
  while ((graph != nullptr) && (graph->GetParentGraph() != nullptr)) {
    graph = graph->GetParentGraph();
  }
  return (graph == nullptr) ? "" : graph->GetName();
}

Status SetOm2CompatibleOmInfoList(const GeModelPtr &ge_model) {
  std::vector<int64_t> om_info;
  om_info.push_back(static_cast<int64_t>(ge_model->GetWeightSize()));
  om_info.push_back(static_cast<int64_t>(ge_model->GetTBEKernelStore().DataSize()));
  om_info.push_back(static_cast<int64_t>(ge_model->GetCustAICPUKernelStore().DataSize()));
  const auto &task_def = ge_model->GetModelTaskDefPtr();
  om_info.push_back(task_def != nullptr ? static_cast<int64_t>(task_def->ByteSizeLong()) : 0);
  // Keep the OM om_info_list schema for JSON compatibility. OM2 stores resources as ZIP entries and does not
  // build the legacy SO_STORE partition, so so_store_size is recorded as 0.
  om_info.push_back(0);
  GE_CHK_BOOL_EXEC(ge::AttrUtils::SetListInt(*(ge_model.get()), "om_info_list", om_info),
                   GELOGE(FAILED, "[OM2] SetListInt of om_info_list failed.");
                   return FAILED);
  return SUCCESS;
}
}  // namespace

Status Om2PackageHelper::SaveToOmRootModel(const GeRootModelPtr &ge_root_model, const std::string &output_file,
                                           ModelBufferData &model, const bool is_unknown_shape) {
  GE_ASSERT_NOTNULL(ge_root_model, "[OM2] ge_root_model is nullptr");
  GE_ASSERT_TRUE(!output_file.empty(), "[OM2] Empty path of output file is invalid");
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  GE_ASSERT_TRUE(!name_to_ge_model.empty(), "[OM2] No subgraphs found in ge_root_model");

  if (!is_unknown_shape) {
    auto &model_root = name_to_ge_model.begin()->second;
    return SaveToOmModel(model_root, output_file, model, ge_root_model);
  }

  // todo 动态 shape 场景暂时不支持
  GELOGE(FAILED, "[OM2] Unknown shape models are not supported for .om2 format conversion");
  (void)REPORT_PREDEFINED_ERR_MSG(
      "E10055", std::vector<const char *>({"reason"}),
      std::vector<const char *>({"Unknown shape models are not supported for .om2 format conversion"}));
  return FAILED;
}

Status Om2PackageHelper::SaveToOmModel(const GeModelPtr &ge_model, const std::string &output_file,
                                       ModelBufferData &model, const GeRootModelPtr &ge_root_model) {
  GE_ASSERT_NOTNULL(ge_model, "ge_model is nullptr");
  GE_ASSERT_TRUE(!output_file.empty(), "[OM2] Empty path of the output file is invalid");

  // Set model-level attrs for OM2 JSON compatibility.
  const bool set_atc_cmdline =
      ge::AttrUtils::SetStr(*(ge_model.get()), ATTR_MODEL_ATC_CMDLINE, domi::GetContext().atc_cmdline);
  GE_CHK_BOOL_EXEC(set_atc_cmdline,
                   GELOGE(FAILED, "[OM2] SetStr for atc_cmdline failed.");
                   return FAILED);
  std::string opp_version;
  std::string opp_path;
  (void)PluginManager::GetOppPath(opp_path);
  const std::string version_path = opp_path + "/version.info";
  if ((!PluginManager::GetVersionFromPath(version_path, opp_version)) ||
      (!ge::AttrUtils::SetStr(*(ge_model.get()), ATTR_MODEL_OPP_VERSION, opp_version))) {
    GELOGW("[OM2] Ge model set opp version unsuccessful!");
  }

  const std::string writer_path = (!is_offline_ && !ge_model->GetName().empty()) ? ge_model->GetName() : output_file;
  auto zip_writer = MakeShared<ZipArchiveWriter>(writer_path);
  GE_ASSERT_NOTNULL(zip_writer);
  GE_ASSERT_TRUE(zip_writer->IsMemFileOpened());
  std::vector<Om2ConstMeta> const_metas;

  // 1. Codegen and shared library
  GE_ASSERT_SUCCESS(SaveCodegenArtifacts(zip_writer, ge_model, 0UL, const_metas));
  // 2. Save constants/weights
  GE_ASSERT_SUCCESS(SaveConstants(zip_writer, ge_model, 0UL, const_metas, !is_offline_));
  // 3. Save TBE kernels
  GE_ASSERT_SUCCESS(SaveTbeKernels(zip_writer, ge_model));
  // 4. Save cust AI cpu kernels
  GE_ASSERT_SUCCESS(SaveCustAICpuKernels(zip_writer, ge_model));
  // 5. Save meta infos of the compiled model
  GE_ASSERT_SUCCESS(SaveModelInfo(zip_writer, ge_model, 0UL));
  // 6. Save operator attributes
  GE_ASSERT_SUCCESS(SaveOpAttrJson(zip_writer, ge_model, 0UL));
  // 7. Save graph debug files (onnx pbtxt + ge proto txt)
  GE_ASSERT_SUCCESS(SaveGraphDebugFiles(zip_writer, ge_model, 0UL));
  // 8. Save archive manifest
  GE_ASSERT_SUCCESS(SaveManifest(zip_writer, ge_root_model));

  // Complete packaging
  GE_ASSERT_TRUE(zip_writer->SaveModelData(model, is_offline_));
  GELOGI("[OM2] Successfully created OM2 model");

  return SUCCESS;
}

void Om2PackageHelper::SetSaveMode(const bool val) {
  is_offline_ = val;
}

Status Om2PackageHelper::RelocateExternalWeights(const std::string &output_file_name, const ModelBufferData &model,
                                                 ModelBufferData &relocated_model, bool &relocated) {
  relocated = false;
  SimpleZipArchiveReader archive(model.data.get(), model.length);
  if (!archive.IsGood()) {
    GELOGW("[OM2] Model buffer has zip magic but is not a valid zip archive, save original buffer.");
    return SUCCESS;
  }
  const auto archive_entries = archive.ListFiles();
  std::map<std::string, std::string> rewritten_configs;
  std::map<std::string, std::string> old_file_to_new_file;
  GE_ASSERT_SUCCESS(CollectOm2ExternalWeightRelocation(output_file_name, archive, archive_entries, rewritten_configs,
                                                       old_file_to_new_file));
  if (old_file_to_new_file.empty()) {
    return SUCCESS;
  }
  GE_ASSERT_SUCCESS(RepackOm2Model(output_file_name, archive, archive_entries, rewritten_configs, relocated_model));
  GE_ASSERT_SUCCESS(FileConstantUtils::MoveExternalWeightFiles(old_file_to_new_file));
  relocated = true;
  return SUCCESS;
}

Status Om2PackageHelper::ExtractGraphProtoTxt(const void *model_data, size_t model_len, std::string &txt_out) {
  GE_ASSERT_NOTNULL(model_data, "[OM2] model_data is nullptr");
  GE_ASSERT_TRUE(model_len > 0U, "[OM2] model_len is 0");

  const auto *data = static_cast<const uint8_t *>(model_data);
  SimpleZipArchiveReader reader(data, model_len);
  GE_ASSERT_TRUE(reader.IsGood(), "[OM2] Failed to open OM2 ZIP archive");

  // Find ge_proto_*.txt entry under debug/ directory
  constexpr const char *kProtoPrefix = "debug/ge_proto_";
  constexpr const char *kProtoSuffix = ".txt";
  const auto file_list = reader.ListFiles();
  std::string entry_path;
  for (const auto &f : file_list) {
    if (f.find(kProtoPrefix) != std::string::npos &&
        f.size() >= strlen(kProtoSuffix) &&
        f.compare(f.size() - strlen(kProtoSuffix), strlen(kProtoSuffix), kProtoSuffix) == 0) {
      entry_path = f;
      break;
    }
  }
  GE_ASSERT_TRUE(!entry_path.empty(), "[OM2] ge_proto_*.txt not found in OM2 archive");

  size_t txt_size = 0U;
  auto txt_buf = reader.ExtractToMem(entry_path, txt_size);
  GE_ASSERT_NOTNULL(txt_buf, "[OM2] Failed to extract %s from OM2 archive", entry_path.c_str());
  GE_ASSERT_TRUE(txt_size > 0U, "[OM2] Extracted proto txt is empty");

  txt_out.assign(reinterpret_cast<const char *>(txt_buf.get()), txt_size);
  GELOGI("[OM2] Extracted ge_proto txt, entry:%s, size:%zu", entry_path.c_str(), txt_out.size());
  return SUCCESS;
}

Status Om2PackageHelper::SaveConstants(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                       const size_t model_index, const std::vector<Om2ConstMeta> &const_metas,
                                       const bool save_file_path) {
  GELOGI("[OM2] Begin to save model constants");
  bool has_internal_const = false;
  for (const auto &const_meta : const_metas) {
    if (const_meta.type == "INTERNAL") {
      has_internal_const = true;
      break;
    }
  }
  if (has_internal_const && (ge_model->GetWeightSize() > 0)) {
    const auto constant_file_name = FormatOm2Path("%s%s%zu", OM2_CONSTANTS_DIR, OM2_CONSTANTS_FILE_PREFIX, model_index);
    GE_ASSERT_TRUE(
        zip_writer->WriteBytes(constant_file_name, ge_model->GetWeightData(), ge_model->GetWeightSize(), false));
  }

  JsonFile json_file;
  (void)json_file.Set("internal_weight_size", has_internal_const ? ge_model->GetWeightSize() : 0U);
  auto const_json_object = JsonFile::json::object();
  for (const auto &const_meta : const_metas) {
    JsonFile const_info;
    (void)const_info.Set("index", const_meta.index);
    (void)const_info.Set("type", const_meta.type);
    (void)const_info.Set("file_name", const_meta.file_name);
    if (save_file_path && (const_meta.type != "INTERNAL") && !const_meta.file_path.empty()) {
      (void)const_info.Set("file_path", const_meta.file_path);
    }
    (void)const_info.Set("offset", const_meta.offset);
    (void)const_info.Set("size", const_meta.size);
    (void)const_info.Set("op_name", const_meta.op_name);
    std::string const_key = const_meta.op_name.empty() ? const_meta.file_name : const_meta.op_name;
    if (const_key.empty()) {
      const_key = "constant_" + std::to_string(const_meta.index);
    }
    const_json_object[const_key] = const_info.Raw();
  }
  (void)json_file.Set("consts", const_json_object);
  const std::string constants_json_str = json_file.Dump();
  const auto constants_config_path =
      FormatOm2Path(OM2_CONSTANTS_CONFIG_PATH_FORMAT, std::to_string(model_index).c_str());
  GE_ASSERT_TRUE(
      zip_writer->WriteBytes(constants_config_path, constants_json_str.data(), constants_json_str.size(), false));

  GELOGI("[OM2] Successfully saved model constants, total size = %zu bytes", ge_model->GetWeightSize());
  return SUCCESS;
}

Status Om2PackageHelper::SaveTbeKernels(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model) {
  GELOGI("[OM2] Begin to save TBE kernels");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph);
  const auto &tbe_kernel_store = ge_model->GetTBEKernelStore();
  const auto kernel_bin_dir = FormatOm2Path(OM2_KERNELS_DIR_FORMAT, "npu_arch");
  // todo 这里可以直接解析tbe_kernel_store.Data(), 无需遍历节点
  std::unordered_set<std::string> added_kernels;
  for (const auto &node : graph->GetNodes(graph->GetGraphUnknownFlag())) {
    std::string kernel_name;
    const auto kernel_name_ptr = AttrUtils::GetStr(node->GetOpDesc(), kAttrKernelName);
    if (kernel_name_ptr != nullptr) {
      kernel_name = *kernel_name_ptr;
    }
    const auto node_name = node->GetName();
    auto kernel_bin = tbe_kernel_store.FindKernel(kernel_name);
    if ((kernel_bin != nullptr) && (added_kernels.count(kernel_name) == 0)) {
      GELOGD("[OM2] Save kernel for node [%s], kernel name is [%s]", node_name.c_str(), kernel_name.c_str());
      const auto entry_path = kernel_bin_dir + Om2CodegenUtils::GetKernelNameWithExtension(kernel_name);
      GE_ASSERT_TRUE(zip_writer->WriteBytes(entry_path, kernel_bin->GetBinData(), kernel_bin->GetBinDataSize(), false));
      (void)added_kernels.insert(kernel_name);
    }
    std::string atomic_kernel_name;
    const auto atomic_kernel_name_ptr = AttrUtils::GetStr(node->GetOpDesc(), ATOMIC_ATTR_TBE_KERNEL_NAME);
    if (atomic_kernel_name_ptr != nullptr) {
      atomic_kernel_name = *atomic_kernel_name_ptr;
    }
    if (!atomic_kernel_name.empty()) {
      const auto atomic_kernel_bin = tbe_kernel_store.FindKernel(atomic_kernel_name);
      if ((atomic_kernel_bin != nullptr) && (added_kernels.count(atomic_kernel_name) == 0)) {
        GELOGD("[OM2] Save atomic kernel for node [%s], kernel name is [%s]", node_name.c_str(),
               atomic_kernel_name.c_str());
        const auto entry_path = kernel_bin_dir + Om2CodegenUtils::GetKernelNameWithExtension(atomic_kernel_name);
        GE_ASSERT_TRUE(zip_writer->WriteBytes(entry_path, atomic_kernel_bin->GetBinData(),
                                              atomic_kernel_bin->GetBinDataSize(), false));
        (void)added_kernels.insert(atomic_kernel_name);
      }
    }
  }
  GELOGI("[OM2] Successfully saved all TBE kernels");
  return SUCCESS;
}

Status Om2PackageHelper::SaveCustAICpuKernels(std::shared_ptr<ZipArchiveWriter> &zip_writer,
                                              const GeModelPtr &ge_model) {
  GELOGI("[OM2] Begin to save cust aicpu kernels");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph);
  const auto &cust_aicpu_kernel_store = ge_model->GetCustAICPUKernelStore();
  if (cust_aicpu_kernel_store.DataSize() > 0U) {
    const auto kernel_bin_dir = FormatOm2Path(OM2_KERNELS_DIR_FORMAT, "npu_arch");
    std::unordered_set<std::string> added_kernels;
    for (const auto &node : graph->GetNodes(graph->GetGraphUnknownFlag())) {
      const auto op_desc = node->GetOpDesc();
      GE_IF_BOOL_EXEC(op_desc == nullptr, continue);
      const auto cust_aicpu_kernel = op_desc->TryGetExtAttr(OP_EXTATTR_CUSTAICPU_KERNEL, CustAICPUKernelPtr());
      GE_IF_BOOL_EXEC(cust_aicpu_kernel == nullptr, continue);
      std::string kernel_name = cust_aicpu_kernel->GetName();
      auto kernel_bin = cust_aicpu_kernel_store.FindKernel(kernel_name);
      if ((kernel_bin != nullptr) && (added_kernels.count(kernel_name) == 0)) {
        GELOGD("[OM2] Save kernel for node [%s], kernel name is [%s]", node->GetName().c_str(), kernel_name.c_str());
        const size_t hash_id = std::hash<std::string>{}(
            std::string(reinterpret_cast<const char *>(kernel_bin->GetBinData()), kernel_bin->GetBinDataSize()));
        const auto entry_path = kernel_bin_dir + std::to_string(hash_id) + "_CustAicpuKernel.o";
        GE_ASSERT_TRUE(
            zip_writer->WriteBytes(entry_path, kernel_bin->GetBinData(), kernel_bin->GetBinDataSize(), false));
        (void)added_kernels.insert(cust_aicpu_kernel->GetName());
      }
    }
  }
  GELOGI("[OM2] Successfully saved all CustAICpu kernels");
  return SUCCESS;
}

Status Om2PackageHelper::SaveModelInfo(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                       const size_t model_index) {
  GELOGI("Begin to saved model manifest");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph);
  ModelIoNodes io_nodes;
  GE_ASSERT_SUCCESS(CollectModelIoNodes(graph, io_nodes));

  JsonFile model_meta_info;
  auto input_json_array = JsonFile::json::array();
  GE_ASSERT_SUCCESS(BuildInputJsonArray(io_nodes.input_ops, input_json_array));

  auto output_json_array = JsonFile::json::array();
  std::vector<std::string> out_node_name;
  (void)AttrUtils::GetListStr(ge_model, ATTR_MODEL_OUT_NODES_NAME, out_node_name);
  ModelMetaExtraInfo extra_info;
  GE_ASSERT_SUCCESS(BuildOutputJsonArray(io_nodes.output_ops, out_node_name, output_json_array, extra_info));
  GE_ASSERT_SUCCESS(CollectDynamicBatchInfo(io_nodes.case_ops, extra_info));
  FillModelMetaInfo(ge_model, input_json_array, output_json_array, extra_info, GetRootGraphName(ge_model),
                    model_meta_info);

  const auto model_meta_info_str = model_meta_info.Dump();
  const auto model_meta_entry_path = FormatOm2Path(OM2_MODEL_META_PATH_FORMAT, std::to_string(model_index).c_str());
  GE_ASSERT_TRUE(
      zip_writer->WriteBytes(model_meta_entry_path, model_meta_info_str.data(), model_meta_info_str.size(), false));
  GELOGI("Successfully saved model manifest");
  return SUCCESS;
}

Status Om2PackageHelper::SaveOpAttrJson(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                        const size_t model_index) {
  GELOGI("[OM2] Begin to save operator attributes");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph);

  auto op_attr_object = JsonFile::json::object();
  size_t scanned_op_count = 0UL;
  size_t saved_op_count = 0UL;

  // Traverse all nodes in the graph
  for (const auto &node : graph->GetNodes(graph->GetGraphUnknownFlag())) {
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    ++scanned_op_count;

    // Check if the operator has the _datadump_original_op_names attribute
    std::vector<std::string> original_op_names;
    if (AttrUtils::GetListStr(op_desc, ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES, original_op_names)) {
      // Build the attribute value JSON structure
      auto attr_value_object = JsonFile::json::object();
      attr_value_object["type"] = "LIST_STRING";
      attr_value_object["value"] = original_op_names;

      // Build the operator JSON structure with attribute name as key
      auto op_attr_entry = JsonFile::json::object();
      op_attr_entry[ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES] = attr_value_object;

      // Add to op_attr_object with operator name as top-level key
      op_attr_object[op_desc->GetName()] = op_attr_entry;
      ++saved_op_count;
      GELOGD("[OM2] Saved attr for op [%s], original op names count = %zu", op_desc->GetName().c_str(),
             original_op_names.size());
    }
  }

  // Serialize to JSON string (empty object {} if no operators have the attribute)
  JsonFile op_attr_json(op_attr_object);
  const auto op_attr_json_str = op_attr_json.Dump();
  const auto op_attr_entry_path = FormatOm2Path(OM2_OP_ATTR_PATH_FORMAT, std::to_string(model_index).c_str());
  GE_ASSERT_TRUE(zip_writer->WriteBytes(op_attr_entry_path, op_attr_json_str.data(), op_attr_json_str.size(), false));

  GELOGI("[OM2] Successfully saved operator attributes, scanned %zu ops, saved %zu ops", scanned_op_count,
         saved_op_count);
  return SUCCESS;
}

namespace {
constexpr int32_t kDumpIndexWidth = 8;

std::string MakeDebugFileName(const std::string &prefix, const size_t model_index,
                              const uint32_t graph_id, const std::string &graph_name,
                              const std::string &ext) {
  std::stringstream ss;
  ss << prefix << std::setw(kDumpIndexWidth) << std::setfill('0') << model_index
     << "_graph_" << graph_id << "_" << GetSanitizedName(graph_name) << ext;
  return ss.str();
}

void StripConstWeights(ge::proto::ModelDef &ge_proto) {
  for (int32_t gi = 0; gi < ge_proto.graph_size(); ++gi) {
    auto *graph_def = ge_proto.mutable_graph(gi);
    for (int32_t oi = 0; oi < graph_def->op_size(); ++oi) {
      auto *op_def = graph_def->mutable_op(oi);
      if (op_def->type() == "Const" || op_def->type() == "Constant") {
        op_def->mutable_attr()->erase(ATTR_NAME_WEIGHTS);
      }
    }
  }
}
}  // namespace

Status Om2PackageHelper::SaveOnnxGraphPbtxt(std::shared_ptr<ZipArchiveWriter> &zip_writer,
                                            const ComputeGraphPtr &graph, const size_t model_index) {
  ge::Model model("GE", "");
  model.SetGraph(graph);
  ge::onnx::ModelProto onnx_proto;
  GE_ASSERT_TRUE(OnnxUtils::ConvertGeModelToModelProto(model, onnx_proto, DumpLevel::DUMP_WITH_OUT_DATA),
      "[OM2] Failed to convert ComputeGraph to ONNX ModelProto");

  std::string onnx_str;
  GE_ASSERT_TRUE(google::protobuf::TextFormat::PrintToString(onnx_proto, &onnx_str),
      "[OM2] Failed to convert ONNX ModelProto to pbtxt string");

  const std::string debug_dir = FormatOm2Path(OM2_DEBUG_DIR_FORMAT, std::to_string(model_index).c_str());
  const std::string entry_path = debug_dir +
      MakeDebugFileName("ge_onnx_", model_index, graph->GetGraphID(), graph->GetName(), ".pbtxt");
  GE_ASSERT_TRUE(zip_writer->WriteBytes(entry_path, onnx_str.data(), onnx_str.size(), true),
      "[OM2] Failed to write ONNX pbtxt to ZIP, entry:%s", entry_path.c_str());
  GELOGI("[OM2] Saved ONNX pbtxt, size:%zu, entry:%s", onnx_str.size(), entry_path.c_str());
  return SUCCESS;
}

Status Om2PackageHelper::SaveGeGraphProtoTxt(std::shared_ptr<ZipArchiveWriter> &zip_writer,
                                             const GeModelPtr &ge_model, const size_t model_index) {
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph, "[OM2] ComputeGraph is nullptr");

  // Set om_info_list before copying attrs so OM2 JSON keeps the OM-compatible size summary.
  GE_ASSERT_SUCCESS(SetOm2CompatibleOmInfoList(ge_model));

  const ModelPtr model_tmp = ge::MakeShared<ge::Model>(ge_model->GetName(), ge_model->GetPlatformVersion());
  GE_ASSERT_NOTNULL(model_tmp, "[OM2] Failed to create Model");
  model_tmp->SetGraph(graph);
  model_tmp->SetVersion(ge_model->GetVersion());
  model_tmp->SetAttr(ge_model->MutableAttrMap());

  ge::Buffer model_buffer;
  GE_ASSERT_SUCCESS(model_tmp->SaveWithoutSeparate(model_buffer, false),
      "[OM2] Failed to serialize ComputeGraph to ModelDef buffer");

  ge::proto::ModelDef ge_proto;
  GE_ASSERT_TRUE(ge_proto.ParseFromArray(model_buffer.GetData(), model_buffer.GetSize()),
      "[OM2] Failed to parse ModelDef from buffer");

  StripConstWeights(ge_proto);

  std::string proto_str;
  GE_ASSERT_TRUE(google::protobuf::TextFormat::PrintToString(ge_proto, &proto_str),
      "[OM2] Failed to convert ModelDef to proto txt string");

  const std::string debug_dir = FormatOm2Path(OM2_DEBUG_DIR_FORMAT, std::to_string(model_index).c_str());
  const std::string entry_path = debug_dir +
      MakeDebugFileName("ge_proto_", model_index, graph->GetGraphID(), graph->GetName(), ".txt");
  GE_ASSERT_TRUE(zip_writer->WriteBytes(entry_path, proto_str.data(), proto_str.size(), true),
      "[OM2] Failed to write GE proto txt to ZIP, entry:%s", entry_path.c_str());
  GELOGI("[OM2] Saved GE proto txt, size:%zu, entry:%s", proto_str.size(), entry_path.c_str());
  return SUCCESS;
}

Status Om2PackageHelper::SaveGraphDebugFiles(std::shared_ptr<ZipArchiveWriter> &zip_writer,
                                             const GeModelPtr &ge_model, const size_t model_index) {
  GELOGI("[OM2] Begin to save graph debug files");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph, "[OM2] ComputeGraph is nullptr");
  GE_ASSERT_SUCCESS(SaveOnnxGraphPbtxt(zip_writer, graph, model_index));
  GE_ASSERT_SUCCESS(SaveGeGraphProtoTxt(zip_writer, ge_model, model_index));
  return SUCCESS;
}

Status Om2PackageHelper::SaveManifest(std::shared_ptr<ZipArchiveWriter> &zip_writer,
                                      const GeRootModelPtr &ge_root_model) {
  JsonFile json_file;
  (void)json_file.Set<std::string>(OM2_ARCHIVE_VERSION, OM2_ARCHIVE_VERSION_VALUE);
  (void)json_file.Set(OM2_MODEL_NUM, ge_root_model->GetSubgraphInstanceNameToModel().size());
  (void)json_file.Set(OM2_ATC_COMMAND, domi::GetContext().atc_cmdline);
  const auto manifest_str = json_file.Dump();
  GE_ASSERT_TRUE(zip_writer->WriteBytes(OM2_MANIFEST_PATH, manifest_str.data(), manifest_str.size(), false));
  return SUCCESS;
}

Status Om2PackageHelper::SaveCodegenArtifacts(std::shared_ptr<ZipArchiveWriter> &zip_writer, const GeModelPtr &ge_model,
                                              const size_t model_index, std::vector<Om2ConstMeta> &const_metas) {
  GELOGI("[OM2] Begin to save codegen artifacts");
  Om2Codegen codegen;

  Om2CodegenArtifacts artifacts;
  GE_ASSERT_SUCCESS(codegen.Om2CodegenAndCompile(ge_model, artifacts, const_metas));
  GE_ASSERT_TRUE(!artifacts.empty());
  const std::string artifacts_base_dir = FormatOm2Path(OM2_RUNTIME_DIR_FORMAT, std::to_string(model_index).c_str());
  for (const auto &artifact : artifacts) {
    const std::string entry_name = artifacts_base_dir + artifact.file_name;
    GE_ASSERT_TRUE(zip_writer->WriteBytes(entry_name, artifact.data.data(), artifact.data.size(), true),
                   "Failed to write artifact [%s]", artifact.file_name.c_str());
  }
  GELOGI("[OM2] Successfully saved all codegen artifacts");
  return SUCCESS;
}

REGISTER_MODEL_SAVE_HELPER(OM_FORMAT_OM2, Om2PackageHelper);
}  // namespace ge
