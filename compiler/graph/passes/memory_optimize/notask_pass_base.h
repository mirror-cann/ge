/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_PASSES_NOTASK_PASS_BASE_H
#define GE_GRAPH_PASSES_NOTASK_PASS_BASE_H
#include "graph/graph.h"
#include "graph/passes/graph_pass.h"
#include "exe_graph/runtime/shape.h"
#include "transfer_shape_utils.h"

namespace ge {
class NotaskPassBase : public GraphPass {
 public:
  explicit NotaskPassBase(const std::string &lock_attr_name) : lock_attr_name_(lock_attr_name) {}
  ~NotaskPassBase() override = default;
  Status Run(ComputeGraphPtr graph) override;

 protected:
  // 子类必须实现：判断节点类型是否为目标算子
  virtual bool IsTargetOp(const ge::OpDescPtr &op_desc) const = 0;
  // 子类必须实现：轴维度校验（dim 解析 + 调用 CheckDimForInput）
  virtual bool CheckDim(const ge::OpDescPtr &op_desc) const = 0;
  // 子类可选实现：格式校验，默认不过滤
  virtual bool CheckFormat(const ge::OpDescPtr &op_desc) const {
    return true;
  }
  // 子类可选实现：返回算子名用于日志
  virtual std::string GetOpLabel() const {
    return "notask";
  }

  // 轴映射基础设施
  bool GetTransferDims(const ge::OpDescPtr &op_desc, const gert::Shape &src_shape, const int64_t &reshape_type_mask,
                       const ge::GeTensorDesc &input_tensor, transformer::AxisIndexMapping &axis_index_mapping) const;
  bool GetAlignedShape(const ge::OpDescPtr &op_desc, const gert::Shape &src_shape, const int64_t &reshape_type_mask,
                       const ge::GeTensorDesc &input_tensor, gert::Shape &align_shape) const;
  bool CheckDimAlignment(const ge::OpDescPtr &op_desc, const gert::Shape &align_shape, const int64_t dim,
                         const ge::GeShape &ori_shape) const;
  bool CheckRealDim(const gert::Shape &align_shape, const gert::Shape &src_shape,
                    const transformer::AxisIndexMapping &axis_index_mapping, const int64_t &dim,
                    const ge::GeTensorDesc &input_tensor) const;
  bool CheckSplitAxis(const std::vector<int32_t> &src_axes, const int64_t &axis_idx, const int32_t &from_axis,
                      const gert::Shape &align_shape, const gert::Shape &src_shape) const;
  bool IsFromAxisOne(const int64_t &axis_idx, const transformer::AxisIndexMapping &axis_index_mapping,
                     const gert::Shape &align_shape, const gert::Shape &src_shape, const int32_t &from_axis) const;
  bool IsMergedAxisAllOnes(const int64_t &axis_idx, const std::vector<int64_t> &shape) const;
  bool IsFrontDimsAllOnesInMergedAxis(const gert::Shape &align_shape, const gert::Shape &src_shape,
                                      const transformer::AxisIndexMapping &axis_index_mapping, const int64_t &real_dim,
                                      const int64_t &dim) const;
  bool IsFrontDimsAllOnes(const transformer::AxisIndexMapping &axis_index_mapping, const std::vector<int64_t> &shape,
                          const int64_t &real_dim) const;
  void PrintTransferDims(const std::string name, const std::vector<std::vector<int32_t>> &transfer_dims) const;
  void PrintShape(const std::string name, const gert::Shape &shape) const;

  // 输入校验
  bool InputCheck(const ge::NodePtr &node);
  bool CheckTensorAlign(const ge::NodePtr &node, const size_t input_index) const;
  bool HasSameSourceAnchor(const ge::InDataAnchorPtr &in_anchor, std::set<ge::OutDataAnchorPtr> &src_anchors) const;
  bool IsPreNodeTypeValid(const ge::InDataAnchorPtr &in_anchor);
  bool IsPreNodeWithSubgraph(const ge::InDataAnchorPtr &in_anchor) const;
  bool IsPreOutAnchorCanReuse(const ge::OutDataAnchorPtr out_anchor) const;
  bool IsPreOutAnchorValidMultiRef(const ge::OutDataAnchorPtr out_anchor) const;
  bool IsPreNodeAttrValid(const ge::OpDescPtr &pre_op_desc);
  bool IsSameInputMemType(const ge::OpDescPtr &pre_op_desc, const size_t output_idx,
                          std::set<int64_t> &mem_types) const;
  bool IsScalarInput(const ge::NodePtr &node, const size_t input_index) const;

  // Ref节点遍历
  void GetFirstOutAnchorNotInRefNode(const ge::InDataAnchorPtr &input_anchor, ge::OutDataAnchorPtr &src_anchor,
                                     int32_t current_deep) const;
  void GetFirstNotRefNode(const ge::InDataAnchorPtr &input_anchor, ge::NodePtr &node) const;

  // 输出校验
  bool OutputCheck(const ge::NodePtr &node) const;

  // unknown shape校验
  bool IsOwnerGraphUnknown(const ge::NodePtr &node) const;
  bool IsUnknownShapeOp(const ge::OpDescPtr &op_desc) const;

  // LxFusion校验
  bool LxFusionCheck(const ge::NodePtr &node) const;
  bool IsLxFusionMem(const ge::OpDescPtr &op_desc) const;
  bool IsLxFusionOp(const ge::NodePtr &node) const;

  // 单个输入的轴维度校验（公共逻辑：构造src_shape → GetAlignedShape → GetTransferDims → CheckRealDim →
  // CheckDimAlignment）
  bool CheckDimForInput(const ge::OpDescPtr &op_desc, int64_t check_dim, size_t input_idx) const;

  // 设置notask属性
  void SetNotaskAttr(const ge::NodePtr &node) const;
  bool ShouldSkipGraph(const ComputeGraphPtr &graph) const;
  void RunOnTargetNode(const ge::NodePtr &node);

  std::string lock_attr_name_;
  std::string cur_pro_node_name_;
};
}  // namespace ge
#endif
