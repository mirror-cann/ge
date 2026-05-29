/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bg_ext_info.h"
#include "graph_builder/bg_memory.h"
#include "graph_builder/bg_infer_shape.h"
#include "common/debug/ge_log.h"
#include "engine/aicpu/kernel/aicpu_ext_info_handle.h"
#include "engine/node_converter_utils.h"

namespace gert {
namespace bg {
namespace {
ValueHolderPtr GetTempExtInfo() {
  AicpuExtInfoHandler handle("temp", 0U, 0U, ge::DEPEND_IN_SHAPE);
  return bg::ValueHolder::CreateConst(&handle, sizeof(handle));
}
} // namespace

std::vector<ValueHolderPtr> CreatBaseExtInfo(const ge::NodePtr node, const std::string &ext_info) {
  const size_t size = ext_info.size();
  const auto &node_name = node->GetName();
  auto input_num = node->GetInDataNodesAndAnchors().size();
  bool optional_input_placeholder = false;
  (void)ge::AttrUtils::GetBool(node->GetOpDescBarePtr(), kOptionalInputPlaceholder, optional_input_placeholder);
  if (optional_input_placeholder) {
    input_num = node->GetOpDescBarePtr()->GetAllInputsSize();
  }
  const size_t output_num = node->GetAllOutDataAnchorsSize();
  GELOGI("Op %s type %s in all input size is %zu, all input data anchors size is %zu, output_num is %zu.",
         node->GetName().c_str(), ge::NodeUtils::GetNodeType(node).c_str(), node->GetOpDescBarePtr()->GetAllInputsSize(), node->GetInDataNodesAndAnchors().size(), output_num);
  int32_t unknown_shape_type_val = 0;
  (void) ge::AttrUtils::GetInt(node->GetOpDescBarePtr(), ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, unknown_shape_type_val);

  std::vector<ValueHolderPtr> inputs;
  inputs.emplace_back(ValueHolder::CreateConst(ext_info.c_str(), ext_info.size(), true));
  inputs.emplace_back(ValueHolder::CreateConst(&size, sizeof(size)));
  inputs.emplace_back(ValueHolder::CreateConst(node_name.c_str(), node_name.size() + 1, true));
  inputs.emplace_back(ValueHolder::CreateConst(&input_num, sizeof(input_num)));
  inputs.emplace_back(ValueHolder::CreateConst(&output_num, sizeof(output_num)));
  inputs.emplace_back(ValueHolder::CreateConst(&unknown_shape_type_val, sizeof(unknown_shape_type_val)));
  return inputs;
}

ValueHolderPtr BuildExtInfo(const ge::NodePtr node, const std::string &ext_info,
                            const std::vector<ValueHolderPtr> &addrs, const ValueHolderPtr &session_id,
                            const BlockInfo &block_info) {
  if (ext_info.empty()) {
    return GetTempExtInfo();
  }

  auto inputs = CreatBaseExtInfo(node, ext_info);
  inputs.insert(inputs.cend(), addrs.cbegin(), addrs.cend());
  inputs.emplace_back(session_id);
  inputs.emplace_back(block_info.is_block_op);
  inputs.emplace_back(block_info.event_id);
  return ValueHolder::CreateSingleDataOutput("BuildExtInfoHandle", inputs);
}

std::vector<ValueHolderPtr> ExpandAicpuOptionalInputShapes(const ge::NodePtr &node,
                                                           const std::vector<ValueHolderPtr> &input_shapes) {
  GE_ASSERT_NOTNULL(node);
  GE_ASSERT_NOTNULL(node->GetOpDescBarePtr());
  const auto op_desc = node->GetOpDescBarePtr();
  const auto input_num = op_desc->GetAllInputsSize();
  GE_ASSERT_TRUE(input_shapes.size() <= input_num);
  if (input_shapes.size() == input_num) {
    GELOGI("Op[%s] input shapes size %zu already matches all input size %zu, no need expand optional inputs.",
           node->GetName().c_str(), input_shapes.size(), input_num);
    return input_shapes;
  }

  std::vector<ValueHolderPtr> expanded_input_shapes;
  expanded_input_shapes.reserve(input_num);
  size_t actual_input_index = 0U;
  for (size_t input_index = 0U; input_index < input_num; ++input_index) {
    if ((op_desc->MutableInputDesc(static_cast<uint32_t>(input_index)) == nullptr) ||
        (actual_input_index >= input_shapes.size())) {
      expanded_input_shapes.emplace_back(
          NodeConverterUtils::CreateOutputShape(op_desc->GetInputDescPtr(static_cast<uint32_t>(input_index))));
      continue;
    }
    expanded_input_shapes.emplace_back(input_shapes[actual_input_index++]);
  }
  GE_ASSERT_TRUE(actual_input_index == input_shapes.size());
  return expanded_input_shapes;
}

ValueHolderPtr UpdateExtInfo(const ge::OpDescPtr &op_desc, const ExtShapeInfo &ext_shape_info,
                             const ValueHolderPtr &ext_handle, const ValueHolderPtr &stream) {
  if (!IsUnkownShape(op_desc)) {
    return nullptr;
  }
  GELOGI("Op[%s] is unknown shape, start to lowering update ext info.", op_desc->GetName().c_str());
  std::vector<ValueHolderPtr> inputs;
  auto input_num = ext_shape_info.input_shapes.size();
  auto output_num = ext_shape_info.output_shapes.size();
  inputs.emplace_back(ext_handle);
  inputs.emplace_back(ValueHolder::CreateConst(&input_num, sizeof(input_num)));
  inputs.emplace_back(ValueHolder::CreateConst(&output_num, sizeof(output_num)));
  inputs.emplace_back(stream);
  inputs.insert(inputs.cend(), ext_shape_info.input_shapes.cbegin(), ext_shape_info.input_shapes.cend());
  inputs.insert(inputs.cend(), ext_shape_info.output_shapes.cbegin(), ext_shape_info.output_shapes.cend());
  auto update_handle = ValueHolder::CreateSingleDataOutput("UpdateExtShape", inputs);
  return update_handle;
}
} // bg
} // gert
