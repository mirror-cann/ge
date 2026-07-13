/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/helper/visual_json_converter.h"
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include "common/ge_common/debug/log.h"
#include "graph/model.h"

namespace ge {
using Json = nlohmann::json;
using FD = google::protobuf::FieldDescriptor;

// ListValue 元素类型枚举，对应 proto 中 val_type 的取值。
enum class ListValType : int {
  kNone = 0,
  kString = 1,
  kInt = 2,
  kFloat = 3,
  kBool = 4,
  kBytes = 5,
  kTensorDesc = 6,
  kTensor = 7,
  kGraph = 8,
  kNamedAttrs = 9,
  kDataType = 10,
};

// visual.json 的外层包装：{format, format_version, model}。
struct VisualJsonModel {
  std::string format = "ge_visual_json";
  uint32_t format_version = 1U;
  nlohmann::json model;
};

namespace {
// OpDef 中在 visual JSON 不输出的字段（编译期连接信息）。
const std::set<std::string> kOpDefPruneFields = {"src_name", "src_index", "dst_name", "dst_index"};
constexpr int32_t kJsonIndent = 2;
constexpr uint32_t kMaxVisualJsonDepth = 256U;

bool IsMessageOfType(const google::protobuf::Descriptor *desc, const std::string &type_name) {
  return desc != nullptr && desc->name() == type_name;
}

struct ListTypeEntry {
  ListValType val_type;
  const char *field_name;
  const char *visual_type;
};

struct AttrTypeEntry {
  const char *visual_type;
  const char *field_name;
};

struct MessageTypeEntry {
  const char *message_type;
  ListValType list_val_type;
};

static const ListTypeEntry kListTypeTable[] = {
    {ListValType::kString, "s", "list_string"},
    {ListValType::kInt, "i", "list_int"},
    {ListValType::kFloat, "f", "list_float"},
    {ListValType::kBool, "b", "list_bool"},
    {ListValType::kBytes, "bt", "list_bytes"},
    {ListValType::kTensorDesc, "td", "list_tensor_desc"},
    {ListValType::kTensor, "t", "list_tensor"},
    {ListValType::kGraph, "g", "list_graph"},
    {ListValType::kNamedAttrs, "na", "list_named_attrs"},
    {ListValType::kDataType, "dt", "list_data_type"},
};

// visual AttrDef 中以 {type, value} 表示的非标量 oneof 字段。
static const AttrTypeEntry kAttrTypeTable[] = {
    {"bytes", "bt"},  {"expression", "expression"}, {"list", "list"},
    {"func", "func"}, {"tensor_desc", "td"},        {"tensor", "t"},
    {"graph", "g"},
};

// 需要递归还原的 GE 复合 message；出现在 ListValue 中时可辅助推断 val_type。
static const MessageTypeEntry kMessageTypeTable[] = {
    {"TensorDescriptor", ListValType::kTensorDesc},
    {"TensorDef", ListValType::kTensor},
    {"GraphDef", ListValType::kGraph},
    {"NamedAttrs", ListValType::kNamedAttrs},
};

static constexpr int kListTypeTableSize = sizeof(kListTypeTable) / sizeof(kListTypeTable[0]);
static constexpr int kAttrTypeTableSize = sizeof(kAttrTypeTable) / sizeof(kAttrTypeTable[0]);
static constexpr int kMessageTypeTableSize = sizeof(kMessageTypeTable) / sizeof(kMessageTypeTable[0]);

static const ListTypeEntry *LookupByValType(ListValType vt) {
  for (int i = 0; i < kListTypeTableSize; ++i) {
    if (kListTypeTable[i].val_type == vt) {
      return &kListTypeTable[i];
    }
  }
  return nullptr;
}

static const ListTypeEntry *LookupByVisualType(const std::string &type) {
  for (int i = 0; i < kListTypeTableSize; ++i) {
    if (type == kListTypeTable[i].visual_type) {
      return &kListTypeTable[i];
    }
  }
  return nullptr;
}

static const AttrTypeEntry *LookupAttrType(const std::string &type) {
  for (int i = 0; i < kAttrTypeTableSize; ++i) {
    if (type == kAttrTypeTable[i].visual_type) {
      return &kAttrTypeTable[i];
    }
  }
  return nullptr;
}

static const MessageTypeEntry *LookupMessageType(const std::string &type) {
  for (int i = 0; i < kMessageTypeTableSize; ++i) {
    if (type == kMessageTypeTable[i].message_type) {
      return &kMessageTypeTable[i];
    }
  }
  return nullptr;
}

// 从 proto ListValue 中解析类型入口。优先按 val_type 查表；若 val_type 缺失或不匹配，
// 则扫描所有数据字段取第一个非空的作为兜底，兼容历史手写 AttrDef。
static const ListTypeEntry *ResolveListValueEntry(const google::protobuf::Message &msg,
                                                  const google::protobuf::Descriptor *desc,
                                                  const google::protobuf::Reflection *ref, ListValType val_type,
                                                  const google::protobuf::FieldDescriptor *&data_fd) {
  const auto *typed_entry = LookupByValType(val_type);
  data_fd = (typed_entry != nullptr) ? desc->FindFieldByName(typed_entry->field_name) : nullptr;
  if (data_fd != nullptr && ref->FieldSize(msg, data_fd) > 0) {
    return typed_entry;
  }

  // 兜底：按表顺序扫描字段，取第一个有数据的。
  for (int i = 0; i < kListTypeTableSize; ++i) {
    const auto *field = desc->FindFieldByName(kListTypeTable[i].field_name);
    if (field != nullptr && ref->FieldSize(msg, field) > 0) {
      data_fd = field;
      return &kListTypeTable[i];
    }
  }

  return typed_entry;
}

static bool IsCompositeMessageType(const google::protobuf::Descriptor *desc) {
  return desc != nullptr && LookupMessageType(desc->name()) != nullptr;
}

// FromVisual 方向：判断 JSON 对象是否为 visual 格式的紧凑 ListValue（{type, value}）。
bool IsTypedListValue(const Json &j) {
  if (!j.is_object() || !j.contains("type") || !j.contains("value") || !j["type"].is_string()) {
    return false;
  }
  return LookupByVisualType(j["type"].get<std::string>()) != nullptr;
}

bool ShouldPruneFromOmJson(const std::string &field_name, const std::set<std::string> &black_fields) {
  return black_fields.count(field_name) > 0U;
}

void ConvertEnumJsonValueToName(Json &value, const google::protobuf::FieldDescriptor *field) {
  if (field == nullptr || field->enum_type() == nullptr || !value.is_number_integer()) {
    return;
  }
  const auto *enum_value = field->enum_type()->FindValueByNumber(value.get<int32_t>());
  if (enum_value != nullptr) {
    value = enum_value->name();
  }
}

void ConvertEnumFieldToName(Json &value, const google::protobuf::FieldDescriptor *field) {
  if (field == nullptr || field->type() != FD::TYPE_ENUM) {
    return;
  }
  if (field->is_repeated()) {
    if (!value.is_array()) {
      return;
    }
    for (auto &elem : value) {
      ConvertEnumJsonValueToName(elem, field);
    }
    return;
  }
  ConvertEnumJsonValueToName(value, field);
}

void ConvertDataTypeNumberToName(Json &value) {
  if (!value.is_number_integer()) {
    return;
  }
  const auto data_type = value.get<int32_t>();
  if (proto::DataType_IsValid(data_type)) {
    value = proto::DataType_Name(static_cast<proto::DataType>(data_type));
  }
}

void ConvertListDataTypeNumbersToNames(Json &pb_json, const google::protobuf::Descriptor *desc) {
  if (!IsMessageOfType(desc, "ListValue") || !pb_json.contains("dt") || !pb_json["dt"].is_array()) {
    return;
  }
  for (auto &elem : pb_json["dt"]) {
    ConvertDataTypeNumberToName(elem);
  }
}

void ApplyOmJsonFormat(Json &pb_json, const google::protobuf::Descriptor *desc,
                       const std::set<std::string> &black_fields, bool enum_as_string) {
  if (!pb_json.is_object()) {
    return;
  }
  if (!black_fields.empty()) {
    for (const auto &field : black_fields) {
      (void)pb_json.erase(field);
    }
  }
  if (!enum_as_string || desc == nullptr) {
    return;
  }
  ConvertListDataTypeNumbersToNames(pb_json, desc);
  for (auto &item : pb_json.items()) {
    ConvertEnumFieldToName(item.value(), desc->FindFieldByName(item.key()));
  }
}

// FromVisual 方向：二维数组反包装，还原为 proto 的 list_list_int/list_list_float 嵌套格式。
Json WrapNestedList(const Json &value, const char *attr_field_name, const char *outer_field_name,
                    const char *inner_field_name) {
  Json wrapped = Json::array();
  for (const auto &inner : value) {
    wrapped.push_back({{inner_field_name, inner}});
  }
  return {{attr_field_name, {{outer_field_name, wrapped}}}};
}

bool SerializeIntegerFieldValue(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                const google::protobuf::FieldDescriptor *field, const bool is_repeated,
                                const int repeated_index, Json &out) {
  switch (field->type()) {
    case FD::TYPE_INT64:
    case FD::TYPE_SINT64:
    case FD::TYPE_SFIXED64:
      out = is_repeated ? ref->GetRepeatedInt64(msg, field, repeated_index) : ref->GetInt64(msg, field);
      return true;
    case FD::TYPE_INT32:
    case FD::TYPE_SINT32:
    case FD::TYPE_SFIXED32:
      out = is_repeated ? ref->GetRepeatedInt32(msg, field, repeated_index) : ref->GetInt32(msg, field);
      return true;
    case FD::TYPE_UINT32:
    case FD::TYPE_FIXED32:
      out = is_repeated ? ref->GetRepeatedUInt32(msg, field, repeated_index) : ref->GetUInt32(msg, field);
      return true;
    case FD::TYPE_UINT64:
    case FD::TYPE_FIXED64:
      out = is_repeated ? ref->GetRepeatedUInt64(msg, field, repeated_index) : ref->GetUInt64(msg, field);
      return true;
    default:
      return false;
  }
}

static bool ShouldPruneField(const google::protobuf::FieldDescriptor *field) {
  const auto *parent = field->containing_type();
  if (parent == nullptr) {
    return false;
  }
  const auto &pn = parent->name();
  const auto &fn = field->name();
  if ((pn == "OpDef") && (kOpDefPruneFields.count(fn) > 0)) {
    return true;
  }
  if ((pn == "TensorDef") && (fn == "data")) {
    return true;
  }
  return false;
}

}  // namespace

Status VisualJsonConverter::BuildModelDefFromGeModel(const GeModelPtr &ge_model, proto::ModelDef &model_def) {
  GE_ASSERT_NOTNULL(ge_model, "[VisualJson] GeModel is nullptr");
  const auto &graph = ge_model->GetGraph();
  GE_ASSERT_NOTNULL(graph, "[VisualJson] ComputeGraph is nullptr");

  const ModelPtr model_tmp = ge::MakeShared<ge::Model>(ge_model->GetName(), ge_model->GetPlatformVersion());
  GE_ASSERT_NOTNULL(model_tmp, "[VisualJson] Failed to create Model");
  model_tmp->SetGraph(graph);
  model_tmp->SetVersion(ge_model->GetVersion());
  model_tmp->SetAttr(ge_model->MutableAttrMap());

  ge::Buffer model_buffer;
  GE_ASSERT_SUCCESS(model_tmp->SaveWithoutSeparate(model_buffer, false),
                    "[VisualJson] Failed to serialize ComputeGraph to ModelDef buffer");

  GE_ASSERT_TRUE(model_def.ParseFromArray(model_buffer.GetData(), model_buffer.GetSize()),
                 "[VisualJson] Failed to parse ModelDef from buffer");
  GELOGI("[VisualJson] Built ModelDef: name=%s, graph=%s", model_def.name().c_str(),
         model_def.graph_size() > 0 ? "yes" : "no");
  return SUCCESS;
}

Status VisualJsonConverter::Write(const VisualJsonModel &model, std::string &json_output) {
  try {
    nlohmann::json j;
    j["format"] = model.format;
    j["format_version"] = model.format_version;
    j["model"] = model.model;
    // error_handler_t::replace 处理二进制数据中的非法 UTF-8 字符
    json_output = j.dump(kJsonIndent, ' ', false, nlohmann::json::error_handler_t::replace);
    GELOGD("[VisualJson] Serialized visual JSON, size=%zu bytes", json_output.size());
  } catch (const std::exception &e) {
    GELOGE(FAILED, "[Write] JSON serialization failed: %s", e.what());
    return FAILED;
  }
  return SUCCESS;
}

Status VisualJsonConverter::ConvertModelDef(const proto::ModelDef &model_def, VisualJsonModel &visual_model) {
  // 不经过 Pb2Json 中间 JSON，直接由 protobuf reflection 生成 visual JSON。
  SerializeMessageToVisual(model_def, visual_model.model);
  return SUCCESS;
}

// ToVisual: protobuf to visual.json.
void VisualJsonConverter::SerializeAttrDef(const google::protobuf::Message &msg, nlohmann::json &out) {
  const auto *desc = msg.GetDescriptor();
  const auto *ref = msg.GetReflection();

  // AttrDef 的核心是一个 "value" oneof。
  // 标量类型输出裸值，复杂类型输出 {type, value}，list_list_* 输出二维数组。
  const auto *oneof = desc->FindOneofByName("value");
  if (oneof == nullptr) {
    return;
  }

  const auto *active = ref->GetOneofFieldDescriptor(msg, oneof);
  if (active == nullptr) {
    return;
  }

  switch (active->number()) {
    case proto::AttrDef::kSFieldNumber:
      out = ref->GetString(msg, active);
      break;
    case proto::AttrDef::kIFieldNumber:
      out = ref->GetInt64(msg, active);
      break;
    case proto::AttrDef::kFFieldNumber:
      out = ref->GetFloat(msg, active);
      break;
    case proto::AttrDef::kBFieldNumber:
      out = ref->GetBool(msg, active);
      break;
    case proto::AttrDef::kDtFieldNumber:
      out = ref->GetInt64(msg, active);
      break;
    // list 字段是完整 ListValue 子消息，委托 SerializeListValue 处理。
    case proto::AttrDef::kListFieldNumber:
      SerializeListValue(ref->GetMessage(msg, active), out);
      break;
    default:
      SerializeComplexAttrDef(msg, ref, active, out);
      break;
  }
}

void VisualJsonConverter::SerializeComplexAttrDef(const google::protobuf::Message &msg,
                                                  const google::protobuf::Reflection *ref,
                                                  const google::protobuf::FieldDescriptor *active,
                                                  nlohmann::json &out) {
  auto getBytes = [ref, &msg, active]() -> std::string {
    std::string scratch;
    return ref->GetStringReference(msg, active, &scratch);
  };

  switch (active->number()) {
    case proto::AttrDef::kBtFieldNumber: {
      out = nlohmann::json::object();
      out["type"] = "bytes";
      out["value"] = getBytes();
      break;
    }
    case proto::AttrDef::kExpressionFieldNumber:
      out = nlohmann::json::object();
      out["type"] = "expression";
      out["value"] = getBytes();
      break;
    case proto::AttrDef::kFuncFieldNumber:
      out = nlohmann::json::object();
      out["type"] = "func";
      SerializeMessageToVisual(ref->GetMessage(msg, active), out["value"]);
      break;
    case proto::AttrDef::kTdFieldNumber:
    case proto::AttrDef::kTFieldNumber:
    case proto::AttrDef::kGFieldNumber: {
      const char *type = (active->number() == proto::AttrDef::kTdFieldNumber)  ? "tensor_desc"
                         : (active->number() == proto::AttrDef::kTFieldNumber) ? "tensor"
                                                                               : "graph";
      out = nlohmann::json::object();
      out["type"] = type;
      SerializeMessageToVisual(ref->GetMessage(msg, active), out["value"]);
      break;
    }
    case proto::AttrDef::kListListIntFieldNumber: {
      SerializeNestedNumberList(msg, ref, active, "list_list_i", "list_i", out);
      break;
    }
    case proto::AttrDef::kListListFloatFieldNumber:
      SerializeNestedNumberList(msg, ref, active, "list_list_f", "list_f", out);
      break;
    default:
      break;
  }
}

void VisualJsonConverter::SerializeListValue(const google::protobuf::Message &msg, nlohmann::json &out) {
  const auto *desc = msg.GetDescriptor();
  const auto *ref = msg.GetReflection();

  // ListValue → visual {type, value}，省略 val_type 枚举。
  const auto *val_type_fd = desc->FindFieldByName("val_type");
  if (val_type_fd == nullptr) {
    out = nlohmann::json::object();
    return;
  }
  const int val_type = ref->GetEnumValue(msg, val_type_fd);
  const auto lvt = static_cast<ListValType>(val_type);

  const google::protobuf::FieldDescriptor *data_fd = nullptr;
  const auto *entry = ResolveListValueEntry(msg, desc, ref, lvt, data_fd);
  if (entry == nullptr || data_fd == nullptr) {
    out = nlohmann::json::object();
    return;
  }

  const int field_size = ref->FieldSize(msg, data_fd);
  nlohmann::json arr = nlohmann::json::array();

  for (int i = 0; i < field_size; ++i) {
    nlohmann::json elem;
    SerializeFieldValue(msg, ref, data_fd, i, elem);
    arr.push_back(elem);
  }

  out = nlohmann::json::object();
  out["type"] = entry->visual_type;
  out["value"] = arr;
}

void VisualJsonConverter::SerializeNestedNumberList(const google::protobuf::Message &msg,
                                                    const google::protobuf::Reflection *ref,
                                                    const google::protobuf::FieldDescriptor *field,
                                                    const char *outer_field_name, const char *inner_field_name,
                                                    nlohmann::json &out) {
  // proto: {list_list_int: [{list_i: [1, 2]}, ...]}  →  visual: [[1, 2], ...]
  out = nlohmann::json::array();
  const auto &outer_msg = ref->GetMessage(msg, field);
  const auto *outer_ref = outer_msg.GetReflection();
  const auto *outer_fd = outer_msg.GetDescriptor()->FindFieldByName(outer_field_name);
  if (outer_fd == nullptr) {
    return;
  }
  for (int i = 0; i < outer_ref->FieldSize(outer_msg, outer_fd); ++i) {
    const auto &inner_msg = outer_ref->GetRepeatedMessage(outer_msg, outer_fd, i);
    const auto *inner_ref = inner_msg.GetReflection();
    const auto *inner_fd = inner_msg.GetDescriptor()->FindFieldByName(inner_field_name);
    nlohmann::json inner = nlohmann::json::array();
    if (inner_fd != nullptr) {
      SerializeRepeatedField(inner_msg, inner_ref, inner_fd, inner);
    }
    out.push_back(inner);
  }
}

void VisualJsonConverter::SerializeMessageToVisual(const google::protobuf::Message &msg, nlohmann::json &out) {
  const auto *desc = msg.GetDescriptor();
  const auto *ref = msg.GetReflection();
  if ((desc == nullptr) || (ref == nullptr)) {
    return;
  }

  // AttrDef / ListValue 有紧凑表达，其余 message 通过 reflection 逐字段展开。
  if (IsMessageOfType(desc, "AttrDef")) {
    SerializeAttrDef(msg, out);
    return;
  }

  if (IsMessageOfType(desc, "ListValue")) {
    SerializeListValue(msg, out);
    return;
  }

  out = nlohmann::json::object();

  for (int i = 0; i < desc->field_count(); ++i) {
    const auto *field = desc->field(i);
    if (field == nullptr) {
      continue;
    }

    if (ShouldPruneField(field)) {
      continue;
    }

    // 跳过空字段，与 proto3 HasField 语义一致。
    if (field->is_repeated()) {
      if (ref->FieldSize(msg, field) == 0) {
        continue;
      }
    } else {
      if (!ref->HasField(msg, field)) {
        continue;
      }
    }

    const std::string &vfn = field->name();

    if (field->is_map()) {
      SerializeMapField(msg, ref, field, out[vfn]);
    } else if (field->is_repeated()) {
      SerializeRepeatedField(msg, ref, field, out[vfn]);
    } else {
      SerializeSingularField(msg, ref, field, out[vfn]);
    }
  }
}

void VisualJsonConverter::SerializeMapField(const google::protobuf::Message &msg,
                                            const google::protobuf::Reflection *ref,
                                            const google::protobuf::FieldDescriptor *field, nlohmann::json &out) {
  // proto map（repeated {key, value} entry）→ {k: v} JSON object。
  out = nlohmann::json::object();
  for (int j = 0; j < ref->FieldSize(msg, field); ++j) {
    const auto &entry = ref->GetRepeatedMessage(msg, field, j);
    const auto *entry_desc = entry.GetDescriptor();
    const auto *entry_ref = entry.GetReflection();
    const auto *key_fd = entry_desc->FindFieldByName("key");
    const auto *val_fd = entry_desc->FindFieldByName("value");
    if (key_fd == nullptr || val_fd == nullptr) {
      continue;
    }

    std::string key_str;
    if (key_fd->type() == FD::TYPE_STRING) {
      key_str = entry_ref->GetString(entry, key_fd);
    } else {
      key_str = std::to_string(entry_ref->GetInt64(entry, key_fd));
    }

    nlohmann::json val_json;
    if (val_fd->type() == FD::TYPE_MESSAGE) {
      SerializeMessageToVisual(entry_ref->GetMessage(entry, val_fd), val_json);
    } else {
      SerializeFieldValue(entry, entry_ref, val_fd, -1, val_json);
    }
    out[key_str] = val_json;
  }
}

void VisualJsonConverter::SerializeRepeatedField(const google::protobuf::Message &msg,
                                                 const google::protobuf::Reflection *ref,
                                                 const google::protobuf::FieldDescriptor *field, nlohmann::json &out) {
  out = nlohmann::json::array();
  for (int j = 0; j < ref->FieldSize(msg, field); ++j) {
    nlohmann::json elem;
    SerializeFieldValue(msg, ref, field, j, elem);
    out.push_back(elem);
  }
}

void VisualJsonConverter::SerializeSingularField(const google::protobuf::Message &msg,
                                                 const google::protobuf::Reflection *ref,
                                                 const google::protobuf::FieldDescriptor *field, nlohmann::json &out) {
  // repeated_index = -1 表示 singular。
  SerializeFieldValue(msg, ref, field, -1, out);
}

void VisualJsonConverter::SerializeFieldValue(const google::protobuf::Message &msg,
                                              const google::protobuf::Reflection *ref,
                                              const google::protobuf::FieldDescriptor *field, int repeated_index,
                                              nlohmann::json &out) {
  const bool is_repeated = repeated_index >= 0;
  if (SerializeIntegerFieldValue(msg, ref, field, is_repeated, repeated_index, out)) {
    return;
  }
  switch (field->type()) {
    case FD::TYPE_MESSAGE:
      SerializeMessageToVisual(
          is_repeated ? ref->GetRepeatedMessage(msg, field, repeated_index) : ref->GetMessage(msg, field), out);
      break;
    case FD::TYPE_BOOL:
      out = is_repeated ? ref->GetRepeatedBool(msg, field, repeated_index) : ref->GetBool(msg, field);
      break;
    case FD::TYPE_FLOAT:
      out = is_repeated ? ref->GetRepeatedFloat(msg, field, repeated_index) : ref->GetFloat(msg, field);
      break;
    case FD::TYPE_DOUBLE:
      out = is_repeated ? ref->GetRepeatedDouble(msg, field, repeated_index) : ref->GetDouble(msg, field);
      break;
    case FD::TYPE_STRING:
      out = is_repeated ? ref->GetRepeatedString(msg, field, repeated_index) : ref->GetString(msg, field);
      break;
    case FD::TYPE_BYTES: {
      std::string scratch;
      out = is_repeated ? ref->GetRepeatedStringReference(msg, field, repeated_index, &scratch)
                        : ref->GetStringReference(msg, field, &scratch);
      break;
    }
    case FD::TYPE_ENUM: {
      const auto *evd = is_repeated ? ref->GetRepeatedEnum(msg, field, repeated_index) : ref->GetEnum(msg, field);
      out = (evd != nullptr) ? evd->name() : "";
      break;
    }
    default:
      break;
  }
}

Json VisualJsonConverter::WrapListValueByType(ListValType val_type, const Json &value) {
  const auto *entry = LookupByValType(val_type);
  Json result = Json::object();
  // val_type 始终输出：OM2 无法区分 proto3 默认值与显式设置，采用安全策略。
  if (val_type != ListValType::kNone) {
    result["val_type"] = static_cast<int>(val_type);
  }
  // 对齐 Pb2Json：仅当数据数组非空时写出数据字段。
  if (entry != nullptr && value.is_array() && !value.empty()) {
    result[entry->field_name] = value;
  }
  return result;
}

bool VisualJsonConverter::WrapTypedAttrDef(const Json &visual_value, Json &wrapped) {
  if (!visual_value.is_object() || !visual_value.contains("type") || !visual_value.contains("value")) {
    return false;
  }
  const std::string &type_str = visual_value["type"].get<std::string>();
  // 先查 list 类型表，"list_string" 等类型映射到 ListValue 字段。
  const auto *entry = LookupByVisualType(type_str);
  if (entry != nullptr) {
    wrapped = {{"list", WrapListValueByType(entry->val_type, visual_value["value"])}};
    return true;
  }
  const auto *attr_entry = LookupAttrType(type_str);
  if (attr_entry != nullptr) {
    wrapped = {{attr_entry->field_name, visual_value["value"]}};
    return true;
  }
  wrapped = visual_value;
  return true;
}

Json VisualJsonConverter::WrapAttrDef(const Json &visual_value) {
  if (visual_value.is_string()) {
    return {{"s", visual_value}};
  }
  if (visual_value.is_number_integer()) {
    return {{"i", visual_value}};
  }
  if (visual_value.is_number_float()) {
    return {{"f", visual_value}};
  }
  if (visual_value.is_boolean()) {
    return {{"b", visual_value}};
  }
  if (visual_value.is_null()) {
    return Json::object();
  }
  Json wrapped;
  if (WrapTypedAttrDef(visual_value, wrapped)) {
    return wrapped;
  }
  // 二维数组按首元素类型判断 list_list_int 或 list_list_float。
  if (visual_value.is_array() && !visual_value.empty() && visual_value[0].is_array()) {
    if (visual_value[0].empty() || visual_value[0][0].is_number_integer()) {
      return WrapNestedList(visual_value, "list_list_int", "list_list_i", "list_i");
    }
    return WrapNestedList(visual_value, "list_list_float", "list_list_f", "list_f");
  }
  return visual_value;
}

Json VisualJsonConverter::WrapListValue(const Json &visual_value, const google::protobuf::Descriptor *list_desc) {
  if (list_desc == nullptr) {
    return visual_value;
  }
  // {type, value} 通过表反查 val_type，再包回 {val_type, field: [...]}。
  if (IsTypedListValue(visual_value)) {
    const auto *entry = LookupByVisualType(visual_value["type"].get<std::string>());
    return WrapListValueByType(entry->val_type, visual_value["value"]);
  }
  if (!visual_value.is_array()) {
    return visual_value;
  }

  // 兜底：无法通过 type 识别时，根据数组首元素推断 val_type。
  ListValType val_type = ListValType::kNone;
  if (visual_value.empty()) {
    return WrapListValueByType(val_type, visual_value);
  }

  const auto &first = visual_value[0];
  if (first.is_string()) {
    val_type = ListValType::kString;
  } else if (first.is_number_integer()) {
    val_type = ListValType::kInt;
  } else if (first.is_number_float()) {
    val_type = ListValType::kFloat;
  } else if (first.is_boolean()) {
    val_type = ListValType::kBool;
  } else if (first.is_object()) {
    for (int i = 0; i < list_desc->field_count(); ++i) {
      const auto *field = list_desc->field(i);
      if (field->is_repeated() && field->type() == FD::TYPE_MESSAGE) {
        const auto *entry = LookupMessageType(field->message_type()->name());
        if (entry != nullptr) {
          val_type = entry->list_val_type;
          break;
        }
      }
    }
  }

  return WrapListValueByType(val_type, visual_value);
}

void VisualJsonConverter::ConvertListValueElementsFromVisual(Json &wrapped,
                                                             const google::protobuf::Descriptor *list_desc,
                                                             uint32_t depth, const std::set<std::string> &black_fields,
                                                             bool enum_as_string) {
  if (!wrapped.is_object()) {
    return;
  }
  // ListValue 内嵌消息元素（td/t/g/na 等字段中的子消息）递归还原。
  // 跳过 val_type（已由 WrapListValueByType 处理），对其余 MESSAGE 类型递归。
  for (auto w_it = wrapped.begin(); w_it != wrapped.end(); ++w_it) {
    if (w_it.key() == "val_type") {
      continue;
    }
    const auto *elem_fd = (list_desc != nullptr) ? list_desc->FindFieldByName(w_it.key()) : nullptr;
    if (elem_fd == nullptr || elem_fd->type() != FD::TYPE_MESSAGE || !w_it.value().is_array()) {
      continue;
    }
    const auto *elem_desc = elem_fd->message_type();
    Json pb_arr = Json::array();
    for (const auto &elem : w_it.value()) {
      if (elem.is_object()) {
        Json pb_elem;
        FromVisualRecursive(elem, pb_elem, elem_desc, depth + 1U, black_fields, enum_as_string);
        pb_arr.push_back(pb_elem);
      } else {
        pb_arr.push_back(elem);
      }
    }
    w_it.value() = pb_arr;
  }
}

void VisualJsonConverter::ConvertAttrDefFieldsFromVisual(Json &wrapped, const google::protobuf::Descriptor *attr_desc,
                                                         uint32_t depth, const std::set<std::string> &black_fields,
                                                         bool enum_as_string) {
  if (!wrapped.is_object()) {
    return;
  }
  // AttrDef oneof 内嵌消息字段（ListValue / NamedAttrs / TensorDescriptor 等）递归还原。
  for (auto w_it = wrapped.begin(); w_it != wrapped.end(); ++w_it) {
    const auto *inner_fd = (attr_desc != nullptr) ? attr_desc->FindFieldByName(w_it.key()) : nullptr;
    if (inner_fd == nullptr || inner_fd->type() != FD::TYPE_MESSAGE) {
      continue;
    }
    if (IsMessageOfType(inner_fd->message_type(), "ListValue")) {
      Json pb_list;
      FromVisualRecursive(w_it.value(), pb_list, inner_fd->message_type(), depth + 1U, black_fields, enum_as_string);
      w_it.value() = pb_list;
    } else if (w_it.value().is_object() && IsCompositeMessageType(inner_fd->message_type())) {
      Json pb_inner;
      FromVisualRecursive(w_it.value(), pb_inner, inner_fd->message_type(), depth + 1U, black_fields, enum_as_string);
      w_it.value() = pb_inner;
    }
  }
}

void VisualJsonConverter::ConvertArrayFromVisual(const Json &visual_json, Json &pb_json,
                                                 const google::protobuf::Descriptor *desc, uint32_t depth,
                                                 const std::set<std::string> &black_fields, bool enum_as_string) {
  pb_json = Json::array();
  for (const auto &elem : visual_json) {
    if (elem.is_object()) {
      Json pb_elem;
      FromVisualRecursive(elem, pb_elem, desc, depth + 1U, black_fields, enum_as_string);
      pb_json.push_back(pb_elem);
    } else {
      pb_json.push_back(elem);
    }
  }
}

bool VisualJsonConverter::ConvertMapFieldFromVisual(const std::string &key, const Json &val,
                                                    const google::protobuf::FieldDescriptor *field, Json &pb_json,
                                                    uint32_t depth, const std::set<std::string> &black_fields,
                                                    bool enum_as_string) {
  if (field == nullptr || !field->is_map() || !val.is_object()) {
    return false;
  }

  const auto *map_entry_desc = field->message_type();
  const auto *value_fd = (map_entry_desc != nullptr) ? map_entry_desc->FindFieldByName("value") : nullptr;
  const auto *value_desc = (value_fd != nullptr) ? value_fd->message_type() : nullptr;
  Json pb_arr = Json::array();
  for (auto entry = val.begin(); entry != val.end(); ++entry) {
    if (value_desc != nullptr) {
      Json pb_val;
      FromVisualRecursive(entry.value(), pb_val, value_desc, depth + 1U, black_fields, enum_as_string);
      pb_arr.push_back({{"key", entry.key()}, {"value", pb_val}});
    } else {
      pb_arr.push_back({{"key", entry.key()}, {"value", entry.value()}});
    }
  }
  pb_json[key] = pb_arr;
  return true;
}

bool VisualJsonConverter::ConvertMessageFieldFromVisual(const std::string &key, const Json &val,
                                                        const google::protobuf::FieldDescriptor *field, Json &pb_json,
                                                        uint32_t depth, const std::set<std::string> &black_fields,
                                                        bool enum_as_string) {
  if (field == nullptr || field->type() != FD::TYPE_MESSAGE) {
    return false;
  }

  const auto *sub_desc = field->message_type();
  if (IsMessageOfType(sub_desc, "AttrDef")) {
    Json wrapped = WrapAttrDef(val);
    ConvertAttrDefFieldsFromVisual(wrapped, sub_desc, depth + 1U, black_fields, enum_as_string);
    ApplyOmJsonFormat(wrapped, sub_desc, black_fields, enum_as_string);
    pb_json[key] = wrapped;
    return true;
  }
  if (IsMessageOfType(sub_desc, "ListValue") && (val.is_array() || IsTypedListValue(val))) {
    Json wrapped = WrapListValue(val, sub_desc);
    ConvertListValueElementsFromVisual(wrapped, sub_desc, depth + 1U, black_fields, enum_as_string);
    ApplyOmJsonFormat(wrapped, sub_desc, black_fields, enum_as_string);
    pb_json[key] = wrapped;
    return true;
  }
  if (val.is_object()) {
    Json pb_val;
    FromVisualRecursive(val, pb_val, sub_desc, depth + 1U, black_fields, enum_as_string);
    pb_json[key] = pb_val;
    return true;
  }
  return false;
}

bool VisualJsonConverter::ConvertRepeatedMessageFieldFromVisual(const std::string &key, const Json &val,
                                                                const google::protobuf::FieldDescriptor *field,
                                                                Json &pb_json, uint32_t depth,
                                                                const std::set<std::string> &black_fields,
                                                                bool enum_as_string) {
  if (field == nullptr || !field->is_repeated() || field->type() != FD::TYPE_MESSAGE || !val.is_array()) {
    return false;
  }

  const auto *sub_desc = field->message_type();
  Json pb_arr;
  ConvertArrayFromVisual(val, pb_arr, sub_desc, depth + 1U, black_fields, enum_as_string);
  pb_json[key] = pb_arr;
  return true;
}

void VisualJsonConverter::ProcessObjectFromVisual(const Json &visual_json, Json &pb_json,
                                                  const google::protobuf::Descriptor *desc, uint32_t depth,
                                                  const std::set<std::string> &black_fields, bool enum_as_string) {
  // FromVisual 逐字段分派，依次处理 map、message、repeated message、enum 和默认字段。
  pb_json = Json::object();
  for (auto it = visual_json.begin(); it != visual_json.end(); ++it) {
    const std::string &key = it.key();
    if (ShouldPruneFromOmJson(key, black_fields)) {
      continue;
    }
    const auto *field = (desc != nullptr) ? desc->FindFieldByName(key) : nullptr;
    const auto &val = it.value();

    // 先还原 proto 复合字段：map、单个 message 字段、repeated message 数组字段。
    if (ConvertMapFieldFromVisual(key, val, field, pb_json, depth + 1U, black_fields, enum_as_string)) {
      continue;
    }
    if (ConvertMessageFieldFromVisual(key, val, field, pb_json, depth + 1U, black_fields, enum_as_string)) {
      continue;
    }
    if (ConvertRepeatedMessageFieldFromVisual(key, val, field, pb_json, depth + 1U, black_fields, enum_as_string)) {
      continue;
    }

    // enum 在 visual JSON 中是名称字符串，回转 protobuf JSON 时需要恢复成数值。
    if (field != nullptr && field->type() == FD::TYPE_ENUM && val.is_string()) {
      if (enum_as_string) {
        pb_json[key] = val;
        continue;
      }
      const auto *enum_desc = field->enum_type();
      const auto *evd = enum_desc->FindValueByName(val.get<std::string>());
      if (evd != nullptr) {
        pb_json[key] = evd->number();
        continue;
      }
    }

    pb_json[key] = val;
  }
  ApplyOmJsonFormat(pb_json, desc, black_fields, enum_as_string);
}

void VisualJsonConverter::FromVisualRecursive(const Json &visual_json, Json &pb_json,
                                              const google::protobuf::Descriptor *desc, uint32_t depth,
                                              const std::set<std::string> &black_fields, bool enum_as_string) {
  if (depth > kMaxVisualJsonDepth) {
    throw std::runtime_error("visual JSON nesting is too deep");
  }
  if (IsMessageOfType(desc, "AttrDef")) {
    pb_json = WrapAttrDef(visual_json);
    ConvertAttrDefFieldsFromVisual(pb_json, desc, depth + 1U, black_fields, enum_as_string);
    ApplyOmJsonFormat(pb_json, desc, black_fields, enum_as_string);
    return;
  }
  if (IsMessageOfType(desc, "ListValue")) {
    pb_json = WrapListValue(visual_json, desc);
    ConvertListValueElementsFromVisual(pb_json, desc, depth + 1U, black_fields, enum_as_string);
    ApplyOmJsonFormat(pb_json, desc, black_fields, enum_as_string);
    return;
  }
  if (visual_json.is_object()) {
    ProcessObjectFromVisual(visual_json, pb_json, desc, depth + 1U, black_fields, enum_as_string);
  } else if (visual_json.is_array()) {
    ConvertArrayFromVisual(visual_json, pb_json, desc, depth + 1U, black_fields, enum_as_string);
  } else {
    pb_json = visual_json;
  }
}

Status VisualJsonConverter::SerializeFromGeModel(const GeModelPtr &ge_model, std::string &json) {
  proto::ModelDef model_def;
  GE_ASSERT_SUCCESS(BuildModelDefFromGeModel(ge_model, model_def));
  return SerializeFromModelDef(model_def, json);
}

Status VisualJsonConverter::SerializeFromModelDef(const proto::ModelDef &model_def, std::string &json) {
  VisualJsonModel visual_model;
  GE_ASSERT_SUCCESS(ConvertModelDef(model_def, visual_model));
  GE_ASSERT_SUCCESS(Write(visual_model, json));
  GELOGD("[VisualJson] SerializeFromModelDef done, model=%s, json_size=%zu", model_def.name().c_str(), json.size());
  return SUCCESS;
}

Status VisualJsonConverter::LoadFromVisualJson(const std::string &visual_json_str,
                                               const std::set<std::string> &black_fields, nlohmann::json &pb_json,
                                               bool enum_as_string) {
  GELOGI("[VisualJson] Loading visual JSON, size=%zu bytes", visual_json_str.size());
  nlohmann::json visual_json;
  try {
    visual_json = nlohmann::json::parse(visual_json_str);
  } catch (const nlohmann::json::parse_error &e) {
    GELOGE(FAILED, "[LoadFromVisualJson] Failed to parse visual JSON: %s", e.what());
    return FAILED;
  } catch (const std::exception &e) {
    GELOGE(FAILED, "[LoadFromVisualJson] Unexpected exception during JSON parse: %s", e.what());
    return FAILED;
  }
  if (!visual_json.is_object() || !visual_json.contains("model")) {
    GELOGE(FAILED, "[LoadFromVisualJson] Invalid visual JSON: missing model field");
    return FAILED;
  }
  try {
    FromVisualRecursive(visual_json["model"], pb_json, proto::ModelDef::descriptor(), 0U, black_fields, enum_as_string);
  } catch (const std::exception &e) {
    GELOGE(FAILED, "[LoadFromVisualJson] Failed to convert visual JSON: %s", e.what());
    return FAILED;
  }
  GELOGI("[VisualJson] LoadFromVisualJson done, pb_json size=%zu bytes", pb_json.dump().size());
  return SUCCESS;
}

Status VisualJsonConverter::LoadFromVisualJson(const std::string &visual_json_str, nlohmann::json &pb_json) {
  const std::set<std::string> black_fields;
  return LoadFromVisualJson(visual_json_str, black_fields, pb_json, false);
}

}  // namespace ge
