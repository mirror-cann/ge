/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_UT_GRAPH_BUILD_INPUT_H2D_OVERLAP_TEST_UTILS_H_
#define GE_UT_GRAPH_BUILD_INPUT_H2D_OVERLAP_TEST_UTILS_H_

#include <cstdint>
#include <limits>
#include <vector>

#include "graph/build/input_h2d_overlap_plan.h"
#include "graph/model.h"
#include "graph/utils/attr_utils.h"

namespace ge {
namespace input_h2d_overlap_test {
using namespace input_h2d_overlap;

constexpr char kPlanAttrName[] = "_ge_input_h2d_overlap_plan";
constexpr const char *kPlanAttrVersion = "version";
constexpr const char *kPlanAttrCopyStreamId = "copy_stream_id";
constexpr const char *kPlanAttrGroups = "groups";
constexpr const char *kPlanAttrInputs = "inputs";
constexpr const char *kPlanAttrWaitPoints = "wait_points";
constexpr const char *kPlanAttrInputIndex = "input_index";
constexpr const char *kPlanAttrSize = "size";
constexpr const char *kPlanAttrStreamId = "stream_id";
constexpr const char *kPlanAttrEventId = "event_id";
constexpr const char *kPlanAttrWaitTaskId = "wait_task_id";

inline bool GetUint64Attr(const NamedAttrs &attrs, const char *const name, uint64_t &value) {
  int64_t raw_value = 0;
  if (!AttrUtils::GetInt(attrs, name, raw_value) || (raw_value < 0)) {
    return false;
  }
  value = static_cast<uint64_t>(raw_value);
  return true;
}

inline bool GetUint32Attr(const NamedAttrs &attrs, const char *const name, uint32_t &value) {
  uint64_t raw_value = 0U;
  if (!GetUint64Attr(attrs, name, raw_value) ||
      (raw_value > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()))) {
    return false;
  }
  value = static_cast<uint32_t>(raw_value);
  return true;
}

inline bool DecodePlan(const NamedAttrs &plan_attr, InputH2DOverlapFinalPlan &plan) {
  plan = InputH2DOverlapFinalPlan();
  std::vector<NamedAttrs> group_attrs;
  if (!GetUint32Attr(plan_attr, kPlanAttrVersion, plan.version) ||
      !GetUint32Attr(plan_attr, kPlanAttrCopyStreamId, plan.copy_stream_id) ||
      !AttrUtils::GetListNamedAttrs(plan_attr, kPlanAttrGroups, group_attrs)) {
    return false;
  }
  plan.groups.reserve(group_attrs.size());
  for (const auto &group_attr : group_attrs) {
    InputH2DOverlapFinalCopyGroup group;
    std::vector<NamedAttrs> input_attrs;
    std::vector<NamedAttrs> wait_point_attrs;
    if (!AttrUtils::GetListNamedAttrs(group_attr, kPlanAttrInputs, input_attrs) ||
        !AttrUtils::GetListNamedAttrs(group_attr, kPlanAttrWaitPoints, wait_point_attrs)) {
      return false;
    }
    group.inputs.reserve(input_attrs.size());
    for (const auto &input_attr : input_attrs) {
      InputH2DOverlapCopyInput input;
      if (!GetUint32Attr(input_attr, kPlanAttrInputIndex, input.input_index) ||
          !GetUint64Attr(input_attr, kPlanAttrSize, input.size)) {
        return false;
      }
      group.inputs.emplace_back(input);
    }
    group.wait_points.reserve(wait_point_attrs.size());
    for (const auto &wait_point_attr : wait_point_attrs) {
      InputH2DOverlapFinalWaitPoint wait_point;
      if (!GetUint32Attr(wait_point_attr, kPlanAttrStreamId, wait_point.stream_id) ||
          !GetUint32Attr(wait_point_attr, kPlanAttrEventId, wait_point.event_id) ||
          !GetUint32Attr(wait_point_attr, kPlanAttrWaitTaskId, wait_point.wait_task_id)) {
        return false;
      }
      group.wait_points.emplace_back(wait_point);
    }
    plan.groups.emplace_back(group);
  }
  return true;
}

inline bool GetPlanAttr(const Model &model, InputH2DOverlapFinalPlan &plan) {
  NamedAttrs plan_attr;
  return AttrUtils::GetNamedAttrs(model, kPlanAttrName, plan_attr) && DecodePlan(plan_attr, plan);
}
}  // namespace input_h2d_overlap_test
}  // namespace ge

#endif  // GE_UT_GRAPH_BUILD_INPUT_H2D_OVERLAP_TEST_UTILS_H_
