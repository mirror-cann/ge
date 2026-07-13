/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// OM2 visual JSON 双向转换器。
// SAVE: GeModel to visual.json for OM2 package.
// LOAD: visual.json to pb_json, aligned with Pb2Json::Message2Json output.

#ifndef BASE_COMMON_HELPER_VISUAL_JSON_CONVERTER_H_
#define BASE_COMMON_HELPER_VISUAL_JSON_CONVERTER_H_

#include <cstdint>
#include <set>
#include <string>
#include "common/model/ge_model.h"
#include "framework/common/ge_inner_error_codes.h"
#include "nlohmann/json.hpp"
#include "proto/ge_ir.pb.h"

namespace ge {
enum class ListValType : int;
struct VisualJsonModel;

class VisualJsonConverter {
 public:
  // GeModel to visual.json string for OM2 package.
  static Status SerializeFromGeModel(const GeModelPtr &ge_model, std::string &json);
  // ModelDef to visual.json string.
  static Status SerializeFromModelDef(const proto::ModelDef &model_def, std::string &json);
  // visual.json to protobuf JSON, aligned with Pb2Json::Message2Json output.
  static Status LoadFromVisualJson(const std::string &visual_json_str, nlohmann::json &pb_json);
  static Status LoadFromVisualJson(const std::string &visual_json_str, const std::set<std::string> &black_fields,
                                   nlohmann::json &pb_json, bool enum_as_string);

 private:
  static Status BuildModelDefFromGeModel(const GeModelPtr &ge_model, proto::ModelDef &model_def);
  static Status ConvertModelDef(const proto::ModelDef &model_def, VisualJsonModel &visual_model);
  static Status Write(const VisualJsonModel &model, std::string &json_output);

  // ToVisual: protobuf to visual.json.
  // 通过 reflection 遍历 message 树，对 AttrDef/ListValue 做紧凑表达。
  static void SerializeMessageToVisual(const google::protobuf::Message &msg, nlohmann::json &visual_json);
  static void SerializeAttrDef(const google::protobuf::Message &msg, nlohmann::json &out);
  static void SerializeComplexAttrDef(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                      const google::protobuf::FieldDescriptor *active, nlohmann::json &out);
  static void SerializeListValue(const google::protobuf::Message &msg, nlohmann::json &out);

  // singular 与 repeated 共用的字段值序列化。repeated_index < 0 表示 singular。
  static void SerializeFieldValue(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                  const google::protobuf::FieldDescriptor *field, int repeated_index,
                                  nlohmann::json &out);
  // list_list_int / list_list_float 二维数组展开。
  static void SerializeNestedNumberList(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                        const google::protobuf::FieldDescriptor *field, const char *outer_field_name,
                                        const char *inner_field_name, nlohmann::json &out);
  // proto map repeated {key, value} to {k: v}.
  static void SerializeMapField(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                const google::protobuf::FieldDescriptor *field, nlohmann::json &out);
  // repeated field to JSON array.
  static void SerializeRepeatedField(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                     const google::protobuf::FieldDescriptor *field, nlohmann::json &out);
  // singular field to JSON scalar or object.
  static void SerializeSingularField(const google::protobuf::Message &msg, const google::protobuf::Reflection *ref,
                                     const google::protobuf::FieldDescriptor *field, nlohmann::json &out);

  // FromVisual: visual.json to protobuf JSON.
  // visual JSON 的紧凑表达还原为 Pb2Json 风格的 protobuf JSON。
  static void FromVisualRecursive(const nlohmann::json &visual_json, nlohmann::json &pb_json,
                                  const google::protobuf::Descriptor *desc, uint32_t depth,
                                  const std::set<std::string> &black_fields, bool enum_as_string);
  static void ProcessObjectFromVisual(const nlohmann::json &visual_json, nlohmann::json &pb_json,
                                      const google::protobuf::Descriptor *desc, uint32_t depth,
                                      const std::set<std::string> &black_fields, bool enum_as_string);
  static bool ConvertMapFieldFromVisual(const std::string &key, const nlohmann::json &val,
                                        const google::protobuf::FieldDescriptor *field, nlohmann::json &pb_json,
                                        uint32_t depth, const std::set<std::string> &black_fields, bool enum_as_string);
  static bool ConvertMessageFieldFromVisual(const std::string &key, const nlohmann::json &val,
                                            const google::protobuf::FieldDescriptor *field, nlohmann::json &pb_json,
                                            uint32_t depth, const std::set<std::string> &black_fields,
                                            bool enum_as_string);
  static bool ConvertRepeatedMessageFieldFromVisual(const std::string &key, const nlohmann::json &val,
                                                    const google::protobuf::FieldDescriptor *field,
                                                    nlohmann::json &pb_json, uint32_t depth,
                                                    const std::set<std::string> &black_fields, bool enum_as_string);
  // visual value to AttrDef oneof wrapper.
  static nlohmann::json WrapAttrDef(const nlohmann::json &visual_value);
  static bool WrapTypedAttrDef(const nlohmann::json &visual_value, nlohmann::json &wrapped);
  // 按 ListValType 将值数组包装为 ListValue 格式。
  static nlohmann::json WrapListValueByType(ListValType val_type, const nlohmann::json &value);
  // visual list to proto ListValue format with type inference fallback.
  static nlohmann::json WrapListValue(const nlohmann::json &visual_value,
                                      const google::protobuf::Descriptor *list_desc);
  // ListValue 内嵌消息元素递归还原。
  static void ConvertListValueElementsFromVisual(nlohmann::json &wrapped, const google::protobuf::Descriptor *list_desc,
                                                 uint32_t depth, const std::set<std::string> &black_fields,
                                                 bool enum_as_string);
  // AttrDef oneof 内嵌消息递归还原。
  static void ConvertAttrDefFieldsFromVisual(nlohmann::json &wrapped, const google::protobuf::Descriptor *attr_desc,
                                             uint32_t depth, const std::set<std::string> &black_fields,
                                             bool enum_as_string);
  // visual 数组元素递归还原。
  static void ConvertArrayFromVisual(const nlohmann::json &visual_json, nlohmann::json &pb_json,
                                     const google::protobuf::Descriptor *desc, uint32_t depth,
                                     const std::set<std::string> &black_fields, bool enum_as_string);
};

}  // namespace ge

#endif  // BASE_COMMON_HELPER_VISUAL_JSON_CONVERTER_H_
