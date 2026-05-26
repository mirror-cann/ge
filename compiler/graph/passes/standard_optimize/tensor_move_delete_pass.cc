/* Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * ===================================================================================================================*/

#include "tensor_move_delete_pass.h"

#include <algorithm>
#include <stack>
#include <unordered_set>
#include <queue>
#include <limits>

#include "graph/utils/graph_utils.h"
#include "graph/utils/attr_utils.h"
#include "common/checker.h"
#include "ge_local_context.h"
#include "node_utils.h"
#include "op_type_utils.h"
#include "common/omg_util/omg_util.h"
#include "framework/common/util.h"
#include "graph/optimize/mem_rw_conflict_optimize.h"
#include "graph/optimize/mem_layout_conflict_optimize/mem_layout_conflict_util.h"

namespace ge {
namespace {
struct TensorMoveDeleteContext {
  NodePtr tensor_move;
  std::vector<std::pair<NodePtr, OutDataAnchorPtr>> path_to_source_node;
  std::vector<std::pair<NodePtr, NodePtr>> pending_control_edges;
  AnchorToSymbol *anchor_to_symbol = nullptr;
  SymbolToAnchors *symbol_to_anchors = nullptr;
  bool has_symbol_table = false;
};
using DeleteRule = std::function<bool(TensorMoveDeleteContext &)>;

bool IsTensorMove(const NodePtr &node) {
  return node->GetType() == TENSORMOVE;
}

/**
 * @brief 判断源节点是否为特殊的数据源节点（可能在其他图中用到）
 * 主要包含以下几类：
 * 1. 变量类 (VARIABLE, VARIABLEV2, REFDATA)
 * 2. 常量类 (CONSTANT, CONSTANTOP, CONSTPLACEHOLDER)
 * 3. 显示引用类 (具有 REF_VAR_SRC_VAR_NAME 属性，指向源变量的节点)
 */
bool IsSourceNodeSpecial(const NodePtr &node) {
  GE_ASSERT_NOTNULL(node);
  GE_ASSERT_NOTNULL(node->GetOpDesc());
  const auto &node_type = node->GetType();
  if (OpTypeUtils::IsVariableNode(node_type) || OpTypeUtils::IsVarLikeNode(node_type) || OpTypeUtils::IsConstNode(node_type)) {
    GELOGI("Node %s of type %s is special node", node->GetName().c_str(), node_type.c_str());
    return true;
  }

  const auto ref_origin_name = AttrUtils::GetStr(node->GetOpDesc(), ge::REF_VAR_SRC_VAR_NAME);
  if (ref_origin_name != nullptr && !ref_origin_name->empty()) {
    GELOGI("Node %s of type %s is special node because it has ref var src name: %s",
           node->GetName().c_str(), node_type.c_str(), ref_origin_name->c_str());
    return true;
  }
  return false;
}

/**
 * @brief 处理子图 DATA 节点的跳出逻辑，将追踪从子图内部切换到定位到父节点对应输入
 * * 查找当前 Data 节点所属的Wrapper Node，将 Wrapper 节点和前驱节点加入路径，并更新追踪锚点与状态标志。
 *
 * @param cur_node        [IN]  当前遇到的子图 DATA 节点
 * @param source_path     [OUT] 路径记录容器，Wrapper 节点会被加入此集合，输出锚点置空
 * @param cur_in_anchor   [OUT] 下一轮迭代的输入锚点，将被更新为父节点对应输入锚点
 * @return Status         SUCCESS: 成功跳出子图并定位到父节点
 *                        FAILED:  获取父节点失败
 */
Status JumpOutFromSubDataToTraceSource(const NodePtr &cur_node, std::vector<std::pair<NodePtr, OutDataAnchorPtr>> &source_path,
                                       InDataAnchorPtr &cur_in_anchor) {
  // 获取 Wrapper 节点的输入锚点 (也就是这一层子图的“入口”)
  const auto parent_in_anchor = NodeUtils::GetParentInDataAnchor(cur_node);
  GE_ASSERT_NOTNULL(parent_in_anchor, "Get parent input anchor failed for DATA node: %s", cur_node->GetName().c_str());

  const auto parent_node = parent_in_anchor->GetOwnerNodeBarePtr();
  GE_ASSERT_NOTNULL(parent_node);

  // 对于从子图 DATA 跳出的 wrapper 节点，仅保留节点标识，不使用 out anchor 参与路径表达。
  source_path.emplace_back(parent_node->shared_from_this(), nullptr);
  cur_in_anchor = parent_in_anchor;

  GELOGI("Jump out of subgraph from DATA %s to parent node %s input index %d.",
         cur_node->GetName().c_str(), parent_node->GetName().c_str(), cur_in_anchor->GetIdx());
  return SUCCESS;
}

/**
 * @brief 根据 ATTR_NAME_PARENT_NODE_INDEX 属性找到映射到当前 PCall 输出端口的输入分支，并将追踪状态更新为子图内部的生产者节点
 *
 * @param cur_node        [IN/OUT] 当前节点。输入时为 PartitionedCall 节点，成功后更新为子图内部的生产者节点
 * @param cur_out_idx     [IN/OUT] 当前输出索引。输入时为 PCall 的输出索引，成功后更新为子图内部节点的输出索引
 * @param source_path     [OUT]    路径记录容器，找到的子图内部节点和对应输出锚点会被加入此路径
 * @param cur_in_anchor   [OUT]    当前输入锚点。将被更新为子图内部节点的输入锚点，在主循环中继续向上回溯
 * @return Status         SUCCESS: 成功找到映射并进入子图
 *                        FAILED:  子图 NetOutput 中无法找到对应当前索引的映射关系
 */
Status JumpInPartitionedCallToTraceSource(const NodePtr &cur_node, int32_t &cur_out_idx,
                                          std::vector<std::pair<NodePtr, OutDataAnchorPtr>> &source_path,
                                          InDataAnchorPtr &cur_in_anchor) {
  const auto sub_graph = NodeUtils::GetSubgraph(*cur_node, 0U);
  GE_ASSERT_NOTNULL(sub_graph);

  const auto sub_graph_netoutput = sub_graph->GetOrUpdateNetOutputNode();
  GE_CHECK_NOTNULL(sub_graph_netoutput);

  for (const auto &in_data_anchor : sub_graph_netoutput->GetAllInDataAnchorsPtr()) {
    int32_t ref_o = -1;
    auto in_desc = sub_graph_netoutput->GetOpDesc()->GetInputDesc(static_cast<uint32_t>(in_data_anchor->GetIdx()));
    GE_ASSERT_TRUE(AttrUtils::GetInt(in_desc, ATTR_NAME_PARENT_NODE_INDEX, ref_o),
                   "Subgraph NetOutput node %s input index %d has no parent node index attr.",
                   sub_graph_netoutput->GetName().c_str(), in_data_anchor->GetIdx());

    if (ref_o == cur_out_idx) {
      // 将cur_in_anchor更新为子图netoutput对应的输入锚点
      auto in_data_anchor_idx = in_data_anchor->GetIdx();
      cur_in_anchor = sub_graph_netoutput->GetInDataAnchor(in_data_anchor_idx);
      // 下一轮将遍历到sub netoutput的前驱，因此这里将sub netoutput和输出锚点加入集合
      source_path.emplace_back(sub_graph_netoutput, in_data_anchor->GetPeerOutAnchor());

      GELOGI("Jump into subgraph %s, from PartitionedCall node %s to sub netoutput node %s index %d",
             sub_graph->GetName().c_str(), cur_node->GetName().c_str(), sub_graph_netoutput->GetName().c_str(), in_data_anchor_idx);
      return SUCCESS;
    }
  }
  GELOGE(FAILED, "Cannot find corresponding sub netoutput and parent node index mapping in subgraph %s for PartitionedCall node %s output index %d",
         sub_graph->GetName().c_str(), cur_node->GetName().c_str(), cur_out_idx);
  return FAILED;
}

void LogTraceRealSourcePath(const NodePtr &start_node, int32_t index,
                            const std::vector<std::pair<NodePtr, OutDataAnchorPtr>> &source_path) {
  if (start_node == nullptr) {
    return;
  }

  std::stringstream ss;
  // 反向遍历：从源头开始打印
  for (auto it = source_path.rbegin(); it != source_path.rend(); ++it) {
    const auto &node = it->first;
    const auto &anchor = it->second;
    if (node != nullptr && anchor != nullptr) {
      ss << node->GetName() << "(out:" << anchor->GetIdx() << ")-->";
    } else if (node != nullptr) {
      ss << node->GetName() << "-->";
    }
  }
  // 最后追加终点（即当前开始回溯的节点）的入边信息
  ss << "(in:" << index << ")" << start_node->GetName();

  GELOGI("Trace reach real source: %s", ss.str().c_str());
}

std::string BuildControlEdgeDesc(const NodePtr &from_node, const NodePtr &to_node) {
  return from_node->GetName() + "(type " + from_node->GetType() + ") -> " +
         to_node->GetName() + "(type " + to_node->GetType() + ")";
}

std::string JoinControlEdgeDescs(const std::vector<std::string> &control_edge_descs) {
  std::stringstream ss;
  for (size_t i = 0U; i < control_edge_descs.size(); ++i) {
    if (i != 0U) {
      ss << "; ";
    }
    ss << control_edge_descs[i];
  }
  return ss.str();
}

/**
 * @brief 从指定节点的输入端口出发，逆向回溯数据流，寻找生成该数据的真正源头节点
 *
 * 核心处理逻辑：
 *
 * 1. 遇到控制流算子终止
 * 若回溯路径上遇到 IF, WHILE, CASE 等控制流算子，视为追踪边界。
 * 此时停止追踪，返回 SUCCESS，并将 source_anchor_index 置为 kInvalidIndex。
 *
 * 2. 子图跳出
 * 若遇到子图内的 DATA 节点（且非根图），说明数据来自父图。
 * 函数会自动跳转到父图中对应的 Wrapper 节点（如 IF 算子）的前驱节点继续回溯。
 *
 * 3. 子图钻入
 * 若遇到 PARTITIONEDCALL 节点，说明数据产生于子图内部。
 * 函数会解析子图结构，钻入子图内部，找到 NetOutput 对应分支的真实生产者。
 *
 * 4. RefOp 透传 / TensorMove
 * 若节点属于RefOp，且存在输入输出的复用关系，
 * 函数会自动跳过该节点，继续沿其复用的输入端口向上回溯。
 *
 * jump_to_prev说明
 * true (默认): 标准回溯模式: 当前 cur_in_anchor 指向的是本层节点的输入，去寻找上游节点作为 cur_node。
 * false: 通常发生在从子图 DATA 节点跳出到父图时，此时 cur_in_anchor 已经被手动更新为父图节点的输入锚点。
 *
 * @param start_node          [IN]  回溯的起始节点。
 * @param index               [IN]  起始节点的输入锚点索引
 * @param source_path         [OUT] 路径记录容器。函数会将从起点到源头过程中经过的所有节点和对应输出锚点加到此列表中（不包含当前TenosrMove节点）
 *
 * @return Status
 * - SUCCESS: 追踪流程正常结束（无论是否找到有效源头，包括遇到控制流算子终止的情况）。
 * - FAILED:  追踪过程中发生错误（如断图、子图映射关系丢失、无法获取锚点等）。
 */
Status TraceRealSourceNode(const NodePtr &start_node, int32_t index, std::vector<std::pair<NodePtr, OutDataAnchorPtr>> &source_path) {
  InDataAnchorPtr cur_in_anchor = start_node->GetInDataAnchor(index);
  while (cur_in_anchor != nullptr) {
    NodePtr cur_node;
    int32_t cur_out_idx;
    OutDataAnchorPtr peer_out_anchor;

    peer_out_anchor = cur_in_anchor->GetPeerOutAnchor();
    if (peer_out_anchor == nullptr) {
      GELOGI("Input anchor index %d of node %s has no peer output anchor.", cur_in_anchor->GetIdx(), start_node->GetName().c_str());
      return SUCCESS;
    }
    cur_node = peer_out_anchor->GetOwnerNode();
    GE_ASSERT_NOTNULL(cur_node);
    cur_out_idx = peer_out_anchor->GetIdx();
    source_path.emplace_back(cur_node, peer_out_anchor);

    // 1. 遇到控制流算子，停止追踪
    if (NodeUtils::IsMultiBranchControlFlowOp(cur_node)) {
      GELOGI("Stop tracing real source for node %s as multi branch control node %s (type: %s) is encountered.",
             start_node->GetName().c_str(), cur_node->GetName().c_str(), cur_node->GetType().c_str());
      return SUCCESS;
    }
    // 2. 处理跨子图跳出 (DATA)
    if (cur_node->GetType() == DATA && !NodeUtils::IsNodeInRootGraph(cur_node)) {
      GE_ASSERT_SUCCESS(JumpOutFromSubDataToTraceSource(cur_node, source_path, cur_in_anchor));
      continue;
    }
    // 3. 处理钻入子图 (PARTITIONEDCALL)
    if (cur_node->GetType() == PARTITIONEDCALL) {
      GE_ASSERT_SUCCESS(JumpInPartitionedCallToTraceSource(cur_node, cur_out_idx, source_path, cur_in_anchor));
      continue;
    }
    // 4. RefOp透传逻辑
    if (peer_out_anchor != nullptr) {
      int32_t reuse_in_idx = -1;
      if (GraphUtils::IsRefFromInput(peer_out_anchor, reuse_in_idx)) {
        GELOGD("RefOp passthrough: node %s out:%d reuses input:%d, continue tracing from that input.",
               cur_node->GetName().c_str(), peer_out_anchor->GetIdx(), reuse_in_idx);
        cur_in_anchor = cur_node->GetInDataAnchor(reuse_in_idx);
        continue;
      }
    }
    // 到达终点，打印路径信息，期望格式: Data(out:0)-->PartitionedCall-->RefOp(out:0)-->(in:0)TensorMove
    LogTraceRealSourcePath(start_node, index, source_path);
    return SUCCESS;
  }
  return FAILED;
}

/**
 * @brief 检查RefOp是否有其他输出复用了同一个输入
 * @param node 当前检查的节点
 * @param current_out_anchor 当前路径正在追踪的输出 Anchor
 * @param tensor_move_name TensorMove 节点名称
 * @return true 存在其他输出复用了同一个输入
 * @return false 不存在其他输出复用了同一个输入
 */
bool HasMultipleOutputsSharingSameInput(const NodePtr &node, const OutDataAnchorPtr &current_out_anchor, const std::string &tensor_move_name) {
  int32_t reuse_in_index = -1;
  // 1. 如果当前 Anchor 本身不是引用输入，直接返回false
  if (!GraphUtils::IsRefFromInput(current_out_anchor, reuse_in_index)) {
    return false;
  }

  // 2. 遍历该节点所有其他输出 Anchor
  for (const auto &tmp_out_anchor : node->GetAllOutDataAnchors()) {
    // 跳过当前正在追踪的这个端口
    if (tmp_out_anchor->GetIdx() == current_out_anchor->GetIdx()) {
      continue;
    }

    // 3. 检查旁路是否引用了同一个输入，且该旁路有下游节点连接
    int32_t tmp_reuse_in_index = -1;
    if (GraphUtils::IsRefFromInput(tmp_out_anchor, tmp_reuse_in_index) &&
        (tmp_reuse_in_index == reuse_in_index) && !tmp_out_anchor->GetPeerInDataAnchorsPtr().empty()) {
      GELOGI("Node %s(type %s) has multiple outputs (Out:%d and Out:%d) sharing input:%d, "
             "and both are connected. Cannot delete tensor move %s.",
             node->GetName().c_str(), node->GetType().c_str(),
             current_out_anchor->GetIdx(), tmp_out_anchor->GetIdx(),
             reuse_in_index, tensor_move_name.c_str());
      return true;
    }
  }

  return false;
}

bool HasRefOutputFromInput(const NodePtr &node, const int32_t input_idx) {
  GE_WARN_ASSERT((node) != nullptr);
  for (const auto &out_anchor : node->GetAllOutDataAnchors()) {
    int32_t reuse_in_idx = -1;
    if (GraphUtils::IsRefFromInput(out_anchor, reuse_in_idx) && (reuse_in_idx == input_idx)) {
      return true;
    }
  }
  return false;
}

bool HasInplaceWriteFromInput(const NodePtr &node, const int32_t input_idx) {
  GE_WARN_ASSERT((node) != nullptr);
  GE_WARN_ASSERT((node->GetOpDesc()) != nullptr);
  for (const auto &output_desc : node->GetOpDesc()->GetAllOutputsDescPtr()) {
    int32_t inplace_input_idx = -1;
    if (AttrUtils::GetInt(output_desc, INPLACE_SUPPORT_INPUT_INDEX, inplace_input_idx) &&
        (inplace_input_idx == input_idx)) {
      return true;
    }
  }
  return false;
}

bool HasAtomicWriteFromInput(const NodePtr &node, const int32_t input_idx) {
  GE_WARN_ASSERT((node) != nullptr);
  GE_WARN_ASSERT((node->GetOpDesc()) != nullptr);
  if (!node->GetOpDesc()->HasAttr(ATOMIC_ATTR_OUTPUT_INDEX)) {
    return false;
  }

  std::vector<int64_t> atomic_output_indexes;
  if (!AttrUtils::GetListInt(node->GetOpDesc(), ATOMIC_ATTR_OUTPUT_INDEX, atomic_output_indexes)) {
    GELOGI("Node %s has attr %s but output indexes parse skipped, treat as source overwrite conservatively.",
           node->GetName().c_str(), ATOMIC_ATTR_OUTPUT_INDEX.c_str());
    return true;
  }
  for (const auto output_idx : atomic_output_indexes) {
    if ((output_idx < 0) || (output_idx > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))) {
      GELOGI("Node %s has invalid atomic output index %ld, treat as source overwrite conservatively.",
             node->GetName().c_str(), output_idx);
      return true;
    }
    const auto out_anchor = node->GetOutDataAnchor(static_cast<int32_t>(output_idx));
    if (out_anchor == nullptr) {
      GELOGI("Node %s atomic output index %ld has no out anchor, treat as source overwrite conservatively.",
             node->GetName().c_str(), output_idx);
      return true;
    }
    int32_t reuse_in_idx = -1;
    if (GraphUtils::IsRefFromInput(out_anchor, reuse_in_idx) && (reuse_in_idx == input_idx)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief 检查"旁路消费者"（sibling consumer，即除 TensorMove 外另一个读同一份源数据的消费节点）
 *        在结构上是否允许删除 TensorMove
 *
 * 这里只卡那些"无论图里有没有其他控制边都不能删"的硬约束：
 *   1. 旁路消费者和 TensorMove 不在同一张子图里。本 pass 的重连和控制边只在单图内做。
 *   2. 旁路消费者类型本身不适合：
 *      - 旁路自身也是 TensorMove（即同一个 source 下挂了两个兄弟 TM）：这条规则的设计
 *        定位是"TM + 一个非 TM 的纯读旁路"，靠补一条"旁路 → TM 后继"的控制边来约束
 *        内存生命周期。如果旁路本身也是个 TM，它自身也应被消除，双 TM 的协同删除和
 *        控制边搬移超出本规则的覆盖范围，保守拒绝不优化。注意：GEPass 的 re-pass 仅在
 *        有节点被删/被改时触发，本轮两个 TM 若都因对方是旁路而被拒，队列为空就退出，
 *        这个场景会永久留着两个 TM，除非更外层的 pass 组合先消化掉其中一个。实际模型
 *        里极少出现同 source 两个 TM，这里 ROI 不高，不单独支持。
 *      - 旁路是 NetOutput：代表全图输出，内存交给调用方，生命周期归框架管，补 ctrl 边
 *        的语义和普通节点不同。
 *      - 旁路是 If / Case / While 等多分支控制流算子：是否真的会读 source 要到运行期才
 *        确定，"旁路先读完再让 TM 后继读"这个假设不一定成立。
 *   3. 旁路消费者是输出复用输入算子。
 *      这种情况下，源内存不仅被旁路消费者自己用，还会通过它的输出继续被下游节点使用。
 *      即使后面让旁路消费者等 TensorMove 的下游先读完再执行，也管不到输出复用输入关系
 *      继续传下去的那段生命周期；内存规划器照样可能提前回收源内存，旁路消费者的下游
 *      就会读到被覆盖的脏数据。
 *
 * @return true 结构上允许删；false 结构上不能删，外部已有的控制边也救不回来。
 */
bool IsSiblingConsumerDeletable(const NodePtr &tensor_move_node, const InDataAnchor *sibling_in_anchor) {
  GE_WARN_ASSERT((tensor_move_node) != nullptr);
  GE_WARN_ASSERT((sibling_in_anchor) != nullptr);
  const auto sibling_node = sibling_in_anchor->GetOwnerNode();
  GE_WARN_ASSERT((sibling_node) != nullptr);
  GE_WARN_ASSERT((sibling_node->GetOpDesc()) != nullptr);

  if (sibling_node->GetOwnerComputeGraph() != tensor_move_node->GetOwnerComputeGraph()) {
    GELOGI("Sibling consumer %s and tensor move %s are not in the same compute graph, keep tensor move.",
           sibling_node->GetName().c_str(), tensor_move_node->GetName().c_str());
    return false;
  }
  if ((sibling_node->GetType() == NETOUTPUT) ||
      NodeUtils::IsMultiBranchControlFlowOp(sibling_node)) {
    GELOGI("Sibling consumer %s(type %s) is NetOutput/control-flow, not supported by "
           "multi-consumer delete rule of tensor move %s.",
           sibling_node->GetName().c_str(), sibling_node->GetType().c_str(), tensor_move_node->GetName().c_str());
    return false;
  }

  const auto sibling_input_idx = sibling_in_anchor->GetIdx();
  if (HasRefOutputFromInput(sibling_node, sibling_input_idx)) {
    GELOGI("Sibling consumer %s(type %s) reuses input %d on output (source memory reaches its "
           "downstream), keep tensor move %s.",
           sibling_node->GetName().c_str(), sibling_node->GetType().c_str(), sibling_input_idx,
           tensor_move_node->GetName().c_str());
    return false;
  }
  return true;
}

/**
 * @brief 检查一个节点是否会通过指定输入端口覆写源节点的内存
 *
 * 覆盖三种覆写情形：
 *   1. 输入读写类型为 kWriteable / kScopeWriteable（ref 输入 / while / modify_input 等）
 *   2. 声明了 inplace 写回：输出 desc 上的 INPLACE_SUPPORT_INPUT_INDEX 指向该输入端口
 *   3. 带 atomic 输出属性，且对应 atomic 输出通过 Ref/ReuseInput 复用该输入端口
 *
 * @return true 会覆写源内存；false 是纯读取，无覆写风险。
 */
bool WillNodeOverwriteSourceMemory(const NodePtr &node, const int32_t source_input_idx) {
  GE_WARN_ASSERT((node) != nullptr);
  GE_WARN_ASSERT((node->GetOpDesc()) != nullptr);

  if (IsNodeInputWritable(node, static_cast<uint32_t>(source_input_idx))) {
    return true;
  }
  if (HasInplaceWriteFromInput(node, source_input_idx)) {
    return true;
  }
  if (HasAtomicWriteFromInput(node, source_input_idx)) {
    return true;
  }
  return false;
}

bool WouldTMSuccessorsOverwriteSource(const TensorMoveDeleteContext &ctx) {
  const auto &tm = ctx.tensor_move;
  const auto tm_out = tm->GetOutDataAnchor(0);
  if (tm_out == nullptr) {
    return true;
  }

  for (const auto succ_in : tm_out->GetPeerInDataAnchorsPtr()) {
    if (succ_in == nullptr) {
      continue;
    }
    const auto succ = succ_in->GetOwnerNode();
    if (succ == nullptr) {
      continue;
    }
    if (WillNodeOverwriteSourceMemory(succ, succ_in->GetIdx())) {
      GELOGI("TM %s successor %s(in:%d) overwrites source memory, cannot delete.",
             tm->GetName().c_str(), succ->GetName().c_str(), succ_in->GetIdx());
      return true;
    }
  }
  return false;
}

/**
 * @brief 判断图里是否已经存在一条从 from 直接连到 to 的控制边
 *
 * 只看单跳的直接控制边，不做沿控制图的多跳可达性搜索：
 *   - 多跳路径存在不等于执行顺序一定被保证，容易误放行；
 *   - 图遍历代价也高；
 *   - 这里只需要"外部是否已经明确写死了先后顺序"，直接边就足够。
 */
bool HasDirectControlOrdering(const NodePtr &from, const NodePtr &to) {
  if ((from == nullptr) || (to == nullptr)) {
    return false;
  }
  const auto from_out_ctrl = from->GetOutControlAnchor();
  const auto to_in_ctrl = to->GetInControlAnchor();
  if ((from_out_ctrl == nullptr) || (to_in_ctrl == nullptr)) {
    return false;
  }
  return from_out_ctrl->IsLinkedWith(to_in_ctrl);
}

/**
 * @brief 沿数据边和控制边的并集做 BFS，判断 from 是否能到达 to
 *
 * 供 WouldCreateControlCycle 复用：考察"若新增 from -> to 的控制边，是否会形成环"时，
 * 等价于检测当前图里 to 是否已经能通过数据边或控制边到达 from。GetOutAllNodes 天然含数据+控制两类后继。
 */
bool IsReachableThroughDataOrControlEdges(const NodePtr &from, const NodePtr &to) {
  if ((from == nullptr) || (to == nullptr)) {
    return false;
  }
  if (from == to) {
    return true;
  }

  std::queue<NodePtr> pending_nodes;
  std::unordered_set<Node *> visited_nodes;
  pending_nodes.push(from);
  visited_nodes.emplace(from.get());
  while (!pending_nodes.empty()) {
    const auto cur_node = pending_nodes.front();
    pending_nodes.pop();
    for (const auto &out_node : cur_node->GetOutAllNodes()) {
      if (out_node == nullptr) {
        continue;
      }
      if (out_node == to) {
        return true;
      }
      if (visited_nodes.emplace(out_node.get()).second) {
        pending_nodes.push(out_node);
      }
    }
  }
  return false;
}

/**
 * @brief 判断新增一条 from -> to 的控制边是否会把图变成有环图
 *
 * 要成环，必然需要 to 已能到达 from（通过数据或控制边），再补一条 from -> to 就闭合成环。
 * 这里只做结构判断，不做实际 AddEdge；调用方据此决定是否保留该 pending 边。
 */
bool WouldCreateControlCycle(const NodePtr &from, const NodePtr &to) {
  if ((from == nullptr) || (to == nullptr)) {
    return false;
  }
  return IsReachableThroughDataOrControlEdges(to, from);
}

/**
 * @brief 把一条 sibling -> tm_succ 的控制边登记到 ctx.pending_control_edges，等 rule 全过后再落地
 *
 * 做两件事：过滤 null 端点、按 (from, to) 去重（避免同一对被重复登记）。
 * 真正的 AddEdge 延迟到 ApplyPendingControlEdges，以保证"任一 rule 拒绝时图保持零改动"。
 */
void AddPendingControlEdge(const NodePtr &from_node, const NodePtr &to_node, TensorMoveDeleteContext &ctx) {
  if ((from_node == nullptr) || (to_node == nullptr)) {
    GELOGW("Pending control edge has null endpoint, skip.");
    return;
  }
  const auto exists = std::find_if(ctx.pending_control_edges.begin(), ctx.pending_control_edges.end(),
                                   [&from_node, &to_node](const std::pair<NodePtr, NodePtr> &edge) {
                                     return (edge.first == from_node) && (edge.second == to_node);
                                   });
  if (exists == ctx.pending_control_edges.end()) {
    ctx.pending_control_edges.emplace_back(from_node, to_node);
  }
}

/**
 * @brief 收集 out_data_anchor 上除 tensor_move 本身外的其他消费者输入锚点
 * @return true 仅当 TM 是该源的直接消费者，且确有 sibling 存在（即满足最基础的多消费者形态）
 *
 * 当前规则只处理最基础的多消费者形态：
 *   source -> sibling
 *   source -> TensorMove -> tm_succ
 * 如果 TensorMove 挂在输出复用输入算子后面（source -> ref_op -> TensorMove），
 * 则 source 的直接消费者是 ref_op 而非 TensorMove，本 pass 不做这类生命周期分析，保守返回 false。
 */
bool CollectSiblingConsumers(const NodePtr &tensor_move_node, const OutDataAnchorPtr &out_data_anchor,
                             std::vector<InDataAnchor *> &sibling_in_anchors) {
  bool tensor_move_is_direct_consumer = false;
  for (const auto peer_in_anchor : out_data_anchor->GetPeerInDataAnchorsPtr()) {
    const auto owner_node = (peer_in_anchor == nullptr) ? nullptr : peer_in_anchor->GetOwnerNodeBarePtr();
    if (owner_node == tensor_move_node.get()) {
      tensor_move_is_direct_consumer = true;
      continue;
    }
    sibling_in_anchors.emplace_back(peer_in_anchor);
  }
  return tensor_move_is_direct_consumer && !sibling_in_anchors.empty();
}

/**
 * @brief 旁路会覆写 source 内存时，对单对 (sibling, tm_succ) 做放行判定
 *
 * 要删 TM 需要保证 "TM 后继先读完、旁路再改写"：
 *   - TM 后继本身也是写者：删 TM 后 tm_succ 会和旁路一起改写 source，语义不等价 → 拒绝。
 *   - 已存在 "TM 后继 → 旁路" 的直接控制边：外部已保序，放行且无需补新边。
 *   - 否则：无保序手段 → 拒绝。
 *
 * @return true 该对允许删（外部已保序）；false 结构上必须保留 TM
 */
bool CanDeleteWhenSiblingOverwritesSource(const NodePtr &tensor_move_node, const NodePtr &sibling_node,
                                          const NodePtr &tensor_move_succ, const int32_t tm_succ_input_idx) {
  if (HasRefOutputFromInput(tensor_move_succ, tm_succ_input_idx)) {
    GELOGI("Keep TM %s: successor %s reuses source input %d on output, source memory reaches downstream.",
           tensor_move_node->GetName().c_str(), tensor_move_succ->GetName().c_str(), tm_succ_input_idx);
    return false;
  }
  if (WillNodeOverwriteSourceMemory(tensor_move_succ, tm_succ_input_idx)) {
    GELOGI("Keep TM %s: sibling %s and successor %s both overwrite source memory.",
           tensor_move_node->GetName().c_str(), sibling_node->GetName().c_str(),
           tensor_move_succ->GetName().c_str());
    return false;
  }
  if (HasDirectControlOrdering(tensor_move_succ, sibling_node)) {
    GELOGI("Delete TM %s: sibling %s overwrites source, existing ctrl %s -> %s ensures read-before-write.",
           tensor_move_node->GetName().c_str(), sibling_node->GetName().c_str(),
           tensor_move_succ->GetName().c_str(), sibling_node->GetName().c_str());
    return true;
  }
  GELOGI("Keep TM %s: sibling %s overwrites source, no ctrl %s -> %s for read-before-write.",
         tensor_move_node->GetName().c_str(), sibling_node->GetName().c_str(),
         tensor_move_succ->GetName().c_str(), sibling_node->GetName().c_str());
  return false;
}

/**
 * @brief 处理一个 sibling 相对全部 TM 后继的约束，必要时登记 sibling -> tm_succ 的待建控制边
 * @return true 该 sibling 结构上允许删除 TM；false 必须保留 TM
 */
bool CheckSiblingAgainstSuccessors(const NodePtr &tensor_move_node, const InDataAnchor *sibling_in_anchor,
                                    const OutDataAnchorPtr &tensor_move_out_anchor, TensorMoveDeleteContext &ctx) {
  if (!IsSiblingConsumerDeletable(tensor_move_node, sibling_in_anchor)) {
    return false;
  }
  const auto sibling_node = sibling_in_anchor->GetOwnerNode();
  const auto sibling_input_idx = sibling_in_anchor->GetIdx();
  const bool sibling_overwrites_source = WillNodeOverwriteSourceMemory(sibling_node, sibling_input_idx);

  for (const auto tensor_move_succ_in_anchor : tensor_move_out_anchor->GetPeerInDataAnchorsPtr()) {
    GE_WARN_ASSERT(tensor_move_succ_in_anchor != nullptr);
    const auto tensor_move_succ = tensor_move_succ_in_anchor->GetOwnerNode();
    GE_WARN_ASSERT((tensor_move_succ) != nullptr);

    if (sibling_overwrites_source) {
      const auto tm_succ_input_idx = tensor_move_succ_in_anchor->GetIdx();
      if (!CanDeleteWhenSiblingOverwritesSource(tensor_move_node, sibling_node, tensor_move_succ,
                                                tm_succ_input_idx)) {
        return false;
      }
      continue;
    }

    if (WouldCreateControlCycle(sibling_node, tensor_move_succ)) {
      GELOGI("Keep TM %s: adding ctrl %s -> %s would create a cycle.",
             tensor_move_node->GetName().c_str(), sibling_node->GetName().c_str(),
             tensor_move_succ->GetName().c_str());
      return false;
    }
    AddPendingControlEdge(sibling_node, tensor_move_succ, ctx);
  }
  return true;
}

/**
 * @brief 多消费者场景下判断能否删除 TensorMove 的主入口
 *
 * 针对 "source 同一输出锚点同时被 TensorMove 和若干 sibling 消费" 的基础形态做结构性放行判定：
 *   1. 先用 CollectSiblingConsumers 过滤，只处理 "TM 直接消费者 + 有 sibling" 的最基础形态；
 *   2. 对每个 sibling 调用 CheckSiblingAgainstSuccessors：
 *      - 检查 sibling 本身类型/同图性/是否输出复用输入等硬约束；
 *      - 对 "sibling 覆写 source" 的情形要求外部已有 tm_succ -> sibling 的直接控制边；
 *      - 其余情形登记 sibling -> tm_succ 的 pending 控制边（延迟在 ApplyPendingControlEdges 落地）。
 *
 * 任一 sibling 不通过即整体放弃删除 TM。
 *
 * @return true 结构上允许继续走删除流程；false 保留 TM
 */
bool TryHandleBasicMultiRefBranch(const NodePtr &tensor_move_node, const OutDataAnchorPtr &out_data_anchor,
                                  TensorMoveDeleteContext &ctx) {
  GE_WARN_ASSERT((tensor_move_node) != nullptr);
  GE_WARN_ASSERT((out_data_anchor) != nullptr);

  std::vector<InDataAnchor *> sibling_in_anchors;
  if (!CollectSiblingConsumers(tensor_move_node, out_data_anchor, sibling_in_anchors)) {
    GELOGI("Skip multi-consumer check: TM %s is not a direct consumer of %s out:%d, or has no sibling.",
           tensor_move_node->GetName().c_str(), out_data_anchor->GetOwnerNode()->GetName().c_str(),
           out_data_anchor->GetIdx());
    return false;
  }

  const auto tensor_move_out_anchor = tensor_move_node->GetOutDataAnchor(0);
  GE_WARN_ASSERT((tensor_move_out_anchor) != nullptr);
  for (const auto &sibling_in_anchor : sibling_in_anchors) {
    if (!CheckSiblingAgainstSuccessors(tensor_move_node, sibling_in_anchor, tensor_move_out_anchor, ctx)) {
      return false;
    }
  }
  GELOGI("TM %s passed multi-consumer check, %zu sibling(s) ordered via existing/new ctrl edges.",
         tensor_move_node->GetName().c_str(), sibling_in_anchors.size());
  return true;
}

/**
 * @brief 检查从TensorMove节点回溯到源节点的路径上，是否所有节点都仅有单一输出
 * @param tensor_move_node 当前的 TensorMove (数据拷贝) 节点
 * @param path_to_source_node 从 TensorMove 回溯到源节点的路径列表，存放节点和对应输出锚点
 * @return true 路径上所有节点都只有单一输出
 * @return false 路径上存在被多处引用的节点
 */
bool IsSourceNodeWithSinglePath(const NodePtr &tensor_move_node,
                                const std::vector<std::pair<NodePtr, OutDataAnchorPtr>> &path_to_source_node,
                                TensorMoveDeleteContext &ctx) {
  GE_WARN_ASSERT(!path_to_source_node.empty());

  for (const auto &pairs : path_to_source_node) {
    const auto &node = pairs.first;
    const auto &out_data_anchor = pairs.second;
    GE_ASSERT_NOTNULL(node);

    // 多分支控制流算子
    if (NodeUtils::IsMultiBranchControlFlowOp(node)) {
      GELOGI("Node %s type %s is multi branch control flow op, cannot delete tensor move %s.", node->GetName().c_str(),
             node->GetType().c_str(), tensor_move_node->GetName().c_str());
      return false;
    }

    // 路径中的某些节点（如 PartitionedCall）仅用于标识跨图路径，不携带可用的 out anchor。
    if (out_data_anchor == nullptr) {
      GELOGI("Node %s(type %s) in source path of tensor move %s has no out data anchor, skip anchor checks.",
             node->GetName().c_str(), node->GetType().c_str(), tensor_move_node->GetName().c_str());
      continue;
    }

    // 多个输出和当前out_data_anchor都引用一个in_data_anchor
    if (HasMultipleOutputsSharingSameInput(node, out_data_anchor, tensor_move_node->GetName())) {
      return false;
    }

    // 单输出多引用
    if (out_data_anchor->GetPeerInDataAnchorsPtr().size() > 1U) {
      if (!TryHandleBasicMultiRefBranch(tensor_move_node, out_data_anchor, ctx)) {
        GELOGI("Out data anchor %d of node %s(type %s) has multiple peer intput data anchors, cannot delete tensor move %s.",
               out_data_anchor->GetIdx(), node->GetName().c_str(), node->GetType().c_str(), tensor_move_node->GetName().c_str());
        return false;
      }
    }
  }
  GELOGI("All nodes in the path from node %s to source node %s pass single-path validation.",
         tensor_move_node->GetName().c_str(), path_to_source_node.back().first->GetName().c_str());
  return true;
}

/**
 * @brief 检查当前源输入节点索引是否在允许的内存复用配置中
 * @param src_out_idx 源节点（通常为 Data 节点）的输出索引，表示第几个输入
 * @param node_name 当前处理的节点名称（用于日志记录）
 * @return 如果该输入索引存在于全局配置中则返回 true，否则返回 false
 */
bool IsMemoryReuseAllowed(int32_t src_out_idx, const std::string &node_name) {
  // Check ge.exec.outputReuseInputMemIndexes first
  std::string output_reuse_input_config;
  if (GetThreadLocalContext().GetOption(OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES, output_reuse_input_config) == SUCCESS) {
    std::vector<std::pair<size_t, size_t>> io_same_addr_pairs;
    ParseOutputReuseInputMemIndexes(output_reuse_input_config, io_same_addr_pairs);

    for (const auto &pair : io_same_addr_pairs) {
      if (pair.first == static_cast<size_t>(src_out_idx)) {
        GELOGI("Memory reuse check passed: input index %d found in ge.exec.outputReuseInputMemIndexes config %s for node %s.",
               src_out_idx, output_reuse_input_config.c_str(), node_name.c_str());
        return true;
      }
    }
  }

  // Check ge.exec.inputReuseMemIndexes
  std::string input_reuse_config;
  if (GetThreadLocalContext().GetOption(OPTION_INPUT_REUSE_MEM_INDEXES, input_reuse_config) == SUCCESS) {
    if (input_reuse_config.empty()) {
      GELOGI("Memory reuse check skipped: ge.exec.inputReuseMemIndexes is empty.");
      return false;
    }
    std::vector<std::string> input_indexes;
    SplitStringByComma(input_reuse_config, input_indexes);
    for (const auto &index_str : input_indexes) {
      const int32_t index = std::stoi(index_str);
      if (index == src_out_idx) {
        GELOGI("Memory reuse check passed: input index %d found in ge.exec.inputReuseMemIndexes config %s for node %s.",
                src_out_idx, input_reuse_config.c_str(), node_name.c_str());
        return true;
      }
    }
  }

  GELOGI("Memory reuse check not pass: input index %d not found in any reuse config, node %s cannot be deleted.",
         src_out_idx, node_name.c_str());
  return false;
}

bool HasReservedAttr(const NodePtr &node) {
  bool reserved = false;
  AttrUtils::GetBool(node->GetOpDesc(),
                     ATTR_NAME_CANNOT_BE_DELETED, reserved);
  if (reserved) {
    GELOGI("TensorMove %s reserved by attr %s.", node->GetName().c_str(), ATTR_NAME_CANNOT_BE_DELETED.c_str());
    return true;
  }
  if (node->GetOpDesc()->HasAttr(ge::ATTR_NO_NEED_CONSTANT_FOLDING)) {
    GELOGI("TensorMove %s reserved by attr %s.", node->GetName().c_str(), ATTR_NO_NEED_CONSTANT_FOLDING.c_str());
    return true;
  }
  return false;
}

/**
* @brief 判断 TensorMove 后继链路是否安全（允许删除 TensorMove 的前提条件）
*
* 逐个检查 TensorMove 输出锚点的所有直接后继节点，要求全部满足：
*   1. 不是 RefOp（输出不复用来自 TensorMove 的那个输入）
*   2. 不是 NetOutput
*   3. 不是 wrapper 算子（PARTITIONEDCALL/STATEFULPARTITIONEDCALL 或多分支控制流算子）
*
* @return true 所有后继都安全；false 存在不安全的后继
*/

bool IsSuccessorSafeAfterTensorMove(const NodePtr &tensor_move_node) {
  GE_ASSERT_NOTNULL(tensor_move_node);
  const auto tm_out_anchor = tensor_move_node->GetOutDataAnchor(0);
  if (tm_out_anchor == nullptr) {
    return true;
  }
  for (const auto peer_in_anchor : tm_out_anchor->GetPeerInDataAnchorsPtr()) {
    if (peer_in_anchor == nullptr) {
      continue;
    }
    const auto succ_node = peer_in_anchor->GetOwnerNode();
    GE_ASSERT_NOTNULL(succ_node);
    const auto &succ_type = succ_node->GetType();
    if (HasRefOutputFromInput(succ_node, peer_in_anchor->GetIdx())) {
      GELOGI("Successor %s(type %s) of TensorMove %s is a RefOp, cannot delete.",
             succ_node->GetName().c_str(), succ_type.c_str(), tensor_move_node->GetName().c_str());
      return false;
    }

    if (succ_type == NETOUTPUT) {
      GELOGI("Successor %s(type %s) of TensorMove %s is NetOutput, cannot delete.",
             succ_node->GetName().c_str(), succ_type.c_str(), tensor_move_node->GetName().c_str());
      return false;
    }
    if (NodeUtils::IsWrapperNode(succ_node)) {
      GELOGI("Successor %s(type %s) of TensorMove %s is a wrapper node with subgraphs, cannot delete.",
             succ_node->GetName().c_str(), succ_type.c_str(), tensor_move_node->GetName().c_str());
      return false;
    }
  }
  return true;
}

DeleteRule CheckPathToSourceNodeValid = [](const TensorMoveDeleteContext &ctx) {
  const auto &path_pairs = ctx.path_to_source_node;
  if (path_pairs.empty()) {
    GELOGI("TensorMove %s has empty path to source node.", ctx.tensor_move->GetName().c_str());
    return false;
  }

  const auto &source_node = path_pairs.back().first;
  if (NodeUtils::IsMultiBranchControlFlowOp(source_node)) {
    GELOGI("Node %s in source path of TensorMove %s is multi branch control node, cannot delete.",
           source_node->GetName().c_str(), ctx.tensor_move->GetName().c_str());
    return false;
  }

  // 源节点是Variable/Const时，需要进一步判断TM后继节点，如果后继节点会修改输入，则TM不能删掉
  if (IsSourceNodeSpecial(source_node)) {
    // TM后继节点是Netoutput或者带有子图时，保守处理，不删除TM
    if (!IsSuccessorSafeAfterTensorMove(ctx.tensor_move)) {
      GELOGI("Source node %s of TensorMove %s is special node and directly connected,"
             " but successor is unsafe, cannot delete.",
             source_node->GetName().c_str(), ctx.tensor_move->GetName().c_str());
      return false;
    }
    if (!WouldTMSuccessorsOverwriteSource(ctx)) {
      GELOGI("Source node %s of TensorMove %s is special but no successor overwrites source, allow deletion check.",
             source_node->GetName().c_str(), ctx.tensor_move->GetName().c_str());
      return true;
    }
    GELOGD("Source node %s of TensorMove %s is special and successor overwrites source, cannot delete.",
           source_node->GetName().c_str(), ctx.tensor_move->GetName().c_str());
    return false;
  }
  return true;
};

DeleteRule CheckSourceNodeReuse = [](const TensorMoveDeleteContext &ctx) {
  const auto src_node = ctx.path_to_source_node.back().first;
  // 源输入是普通计算节点，则无需后续校验
  if (!OpTypeUtils::IsDataNode(src_node->GetType())) {
    return true;
  }

  uint64_t input_index;
  if (!AttrUtils::GetInt(src_node->GetOpDesc(), ATTR_NAME_INDEX, input_index)) {
    GELOGI("Node %s(type %s) does not have attr %s, cannot delete.", src_node->GetName().c_str(), src_node->GetType().c_str(), ATTR_NAME_INDEX.c_str());
    return false;
  }

  // 校验内存复用配置
  if (!IsMemoryReuseAllowed(static_cast<int32_t>(input_index), ctx.tensor_move->GetName())) {
    return false;
  }

  return true;
};

DeleteRule CheckSinglePath = [](TensorMoveDeleteContext &ctx) {
  return IsSourceNodeWithSinglePath(ctx.tensor_move, ctx.path_to_source_node, ctx);
};

DeleteRule CheckRWConflictOnDelete = [](const TensorMoveDeleteContext &ctx) {
  const auto &tm = ctx.tensor_move;
  const auto tm_in = tm->GetInDataAnchor(0);
  const auto tm_out = tm->GetOutDataAnchor(0);
  if ((tm_in == nullptr) || (tm_out == nullptr)) {
    return false;
  }

  const auto peer_out = tm_in->GetPeerOutAnchor();
  const auto src_node = (peer_out != nullptr) ? peer_out->GetOwnerNode() : nullptr;
  if (src_node == nullptr) {
    return false;
  }
  const uint32_t src_out_idx = static_cast<uint32_t>(peer_out->GetIdx());

  for (const auto succ_in : tm_out->GetPeerInDataAnchorsPtr()) {
    const auto tm_succ = (succ_in != nullptr) ? succ_in->GetOwnerNode() : nullptr;
    if (tm_succ == nullptr) {
      continue;
    }
    if (WouldDeleteTensorMoveCauseRWConflict(
            src_node, src_out_idx, tm_succ,
            static_cast<uint32_t>(succ_in->GetIdx()))) {
      GELOGI("Keep TM %s: RW conflict between %s(out:%u) and %s(in:%d).",
             tm->GetName().c_str(), src_node->GetName().c_str(), src_out_idx,
             tm_succ->GetName().c_str(), succ_in->GetIdx());
      return false;
    }
  }
  return true;
};

DeleteRule CheckMemLayoutConflictOnDelete = [](const TensorMoveDeleteContext &ctx) {
  const auto &tm = ctx.tensor_move;
  const auto tm_in = tm->GetInDataAnchor(0);
  const auto tm_out = tm->GetOutDataAnchor(0);
  if ((tm_in == nullptr) || (tm_out == nullptr)) {
    return false;
  }
  if (!ctx.has_symbol_table) {
    return true;
  }

  NodeIndexIO in_io(tm, 0, kIn);
  NodeIndexIO out_io(tm, 0, kOut);
  const auto in_iter = ctx.anchor_to_symbol->find(in_io.ToString());
  const auto out_iter = ctx.anchor_to_symbol->find(out_io.ToString());
  if ((in_iter == ctx.anchor_to_symbol->end()) || (out_iter == ctx.anchor_to_symbol->end())) {
    GELOGW("Symbol not found for TM %s, conservatively keep.", tm->GetName().c_str());
    return false;
  }
  if (in_iter->second == out_iter->second) {
    return true;
  }

  AnchorToSymbol tmp_anchor_to_symbol;
  SymbolToAnchors tmp_symbol_to_anchors;
  MemLayoutConflictUtil::ConstructSingleNodeSymbolTable(in_iter->second, out_iter->second,
      *ctx.anchor_to_symbol, *ctx.symbol_to_anchors,
      tmp_anchor_to_symbol, tmp_symbol_to_anchors);

  auto graph = tm->GetOwnerComputeGraph();
  if (graph == nullptr) {
    return false;
  }
  bool has_conflict = false;
  if (MemLayoutConflictUtil::IsGraphExistMemConflictSymbol(
          graph, tmp_anchor_to_symbol, tmp_symbol_to_anchors, has_conflict) != SUCCESS) {
    GELOGW("IsGraphExistMemConflictSymbol failed for TM %s, conservatively keep.", tm->GetName().c_str());
    return false;
  }
  if (has_conflict) {
    GELOGI("Keep TM %s: mem layout conflict after merging symbols %s and %s.",
           tm->GetName().c_str(), in_iter->second.c_str(), out_iter->second.c_str());
    return false;
  }
  return true;
};

/**
 * @brief 回滚本轮已经加到图里的控制边
 *
 * 与 ApplyPendingControlEdges 配对使用：加边中途失败、或加边成功后 IsolateAndDeleteNode 失败时，
 * 调用方负责把 added_edges 里已经落地的边逐条移除，避免把多余的控制依赖留在图里。
 */
void RollbackAddedControlEdges(
    const std::vector<std::pair<OutControlAnchorPtr, InControlAnchorPtr>> &added_control_edges) {
  for (const auto &added_edge : added_control_edges) {
    (void)GraphUtils::RemoveEdge(added_edge.first, added_edge.second);
  }
}

/**
 * @brief TensorMove 删除成功后的统一日志出口
 *
 * 根据本轮是否新增了控制边，打印两种不同的提示：有新增则把所有新边的 from->to 拼串落日志，
 * 便于事后定位内存生命周期的保序来源；无新增则按"纯冗余拷贝被消除"打简要日志。
 */
void LogTensorMoveDeleted(const NodePtr &node,
                          const std::vector<std::pair<OutControlAnchorPtr, InControlAnchorPtr>> &added_edges,
                          const std::vector<std::string> &added_edge_descs) {
  if (!added_edges.empty()) {
    const auto desc = JoinControlEdgeDescs(added_edge_descs);
    GELOGI("Node %s(type %s) deleted with %zu control edge(s) added: %s.", node->GetName().c_str(),
           node->GetType().c_str(), added_edges.size(), desc.c_str());
    return;
  }
  GELOGI("Node %s(type %s) deleted due to redundant copy.", node->GetName().c_str(), node->GetType().c_str());
}

/**
 * @brief 把 pending_edges 中登记的 sibling -> tm_succ 控制边真正加到图里
 *
 * 已存在的同向边会被跳过（幂等），实际成功加入的边会被追加到 added_edges / added_edge_descs，
 * 便于后续删点失败时调用方调用 RollbackAddedControlEdges 原路回滚。
 *
 * @return SUCCESS 全部处理完毕；FAILED 中途遇到 null 锚点或 AddEdge 失败，日志已由本函数打印，
 *                 回滚由调用方负责。
 */
Status ApplyPendingControlEdges(
    const NodePtr &tensor_move_node,
    const std::vector<std::pair<NodePtr, NodePtr>> &pending_edges,
    std::vector<std::pair<OutControlAnchorPtr, InControlAnchorPtr>> &added_edges,
    std::vector<std::string> &added_edge_descs) {
  added_edges.reserve(pending_edges.size());
  added_edge_descs.reserve(pending_edges.size());
  for (const auto &pending_edge : pending_edges) {
    const auto &from_node = pending_edge.first;
    const auto &to_node = pending_edge.second;
    GE_WARN_ASSERT((from_node) != nullptr);
    GE_WARN_ASSERT((to_node) != nullptr);

    const auto out_ctrl_anchor = from_node->GetOutControlAnchor();
    const auto in_ctrl_anchor = to_node->GetInControlAnchor();
    if ((out_ctrl_anchor == nullptr) || (in_ctrl_anchor == nullptr)) {
      GELOGW("Control anchor is null when deleting tensor move %s: %s -> %s.",
             tensor_move_node->GetName().c_str(), from_node->GetName().c_str(), to_node->GetName().c_str());
      return FAILED;
    }
    if (out_ctrl_anchor->IsLinkedWith(in_ctrl_anchor)) {
      continue;
    }
    if (GraphUtils::AddEdge(out_ctrl_anchor, in_ctrl_anchor) != GRAPH_SUCCESS) {
      GELOGW("Add control edge failed when deleting tensor move %s: %s -> %s, rollback added edges.",
             tensor_move_node->GetName().c_str(), from_node->GetName().c_str(), to_node->GetName().c_str());
      return FAILED;
    }
    added_edges.emplace_back(out_ctrl_anchor, in_ctrl_anchor);
    added_edge_descs.emplace_back(BuildControlEdgeDesc(from_node, to_node));
  }
  return SUCCESS;
}
}  // anonymous namespace

Status TensorMoveDeletePass::Init(const ComputeGraphPtr &compute_graph) {
  GE_CHECK_NOTNULL(compute_graph);
  GE_CHK_STATUS(compute_graph->TopologicalSorting(),
                "TopologicalSorting failed before building symbol table.");
  GE_CHK_STATUS(GraphUtils::GetRefMapping(compute_graph, symbol_to_anchors_, anchor_to_symbol_),
                "GetRefMapping failed.");
  has_symbol_table_ = true;
  return SUCCESS;
}

namespace {
void MergeTensorMoveSymbolAfterDelete(const NodePtr &node, const bool has_symbol_table,
                                      AnchorToSymbol &anchor_to_symbol, SymbolToAnchors &symbol_to_anchors) {
  if (!has_symbol_table) {
    return;
  }
  NodeIndexIO in_io(node, 0, kIn);
  NodeIndexIO out_io(node, 0, kOut);
  const auto &in_io_key = in_io.ToString();
  const auto &out_io_key = out_io.ToString();
  const auto in_iter = anchor_to_symbol.find(in_io_key);
  const auto out_iter = anchor_to_symbol.find(out_io_key);
  const auto input_symbol = (in_iter == anchor_to_symbol.end()) ? std::string() : in_iter->second;
  const auto output_symbol = (out_iter == anchor_to_symbol.end()) ? std::string() : out_iter->second;
  if ((in_iter != anchor_to_symbol.end()) && (out_iter != anchor_to_symbol.end())
      && (in_iter->second != out_iter->second)) {
    GELOGI("Delete TM %s: merge symbol %s into %s.", node->GetName().c_str(),
           out_iter->second.c_str(), in_iter->second.c_str());
    for (auto &node_index_io : symbol_to_anchors[output_symbol]) {
      anchor_to_symbol[node_index_io.ToString()] = input_symbol;
    }
    auto &in_vec = symbol_to_anchors[input_symbol];
    auto &out_vec = symbol_to_anchors[output_symbol];
    in_vec.insert(in_vec.end(), out_vec.begin(), out_vec.end());
    symbol_to_anchors.erase(output_symbol);
  }
  auto &anchors = symbol_to_anchors[input_symbol];
  anchors.erase(std::remove_if(anchors.begin(), anchors.end(), [&in_io_key, &out_io_key](const NodeIndexIO &node_index_io) {
    const auto &anchor_key = node_index_io.ToString();
    return (anchor_key == in_io_key) || (anchor_key == out_io_key);
  }), anchors.end());
  anchor_to_symbol.erase(in_io_key);
  anchor_to_symbol.erase(out_io_key);
}
}

Status TensorMoveDeletePass::Run(NodePtr &node) {
  GE_CHECK_NOTNULL(node);
  GE_CHECK_NOTNULL(node->GetOpDesc());

  if (!IsTensorMove(node) || HasReservedAttr(node)) {
    return SUCCESS;
  }

  TensorMoveDeleteContext ctx{node, {}, {}, &anchor_to_symbol_, &symbol_to_anchors_, has_symbol_table_};

  GE_ASSERT_SUCCESS(TraceRealSourceNode(ctx.tensor_move, 0, ctx.path_to_source_node));
  GE_ASSERT(!ctx.path_to_source_node.empty());
  GE_ASSERT_NOTNULL(ctx.path_to_source_node.back().first);

  std::vector<DeleteRule> rules = {
    CheckPathToSourceNodeValid,
    CheckSourceNodeReuse,
    CheckSinglePath,
    CheckRWConflictOnDelete,
    CheckMemLayoutConflictOnDelete
  };

  for (const auto &rule : rules) {
    if (!rule(ctx)) {
      GELOGD("Node %s(type %s) cannot be deleted.", node->GetName().c_str(), node->GetType().c_str());
      return SUCCESS;
    }
  }

  std::vector<std::pair<OutControlAnchorPtr, InControlAnchorPtr>> added_control_edges;
  std::vector<std::string> added_control_edge_descs;
  if (ApplyPendingControlEdges(node, ctx.pending_control_edges,
                               added_control_edges, added_control_edge_descs) != SUCCESS) {
    RollbackAddedControlEdges(added_control_edges);
    return SUCCESS;
  }

  const auto delete_ret = IsolateAndDeleteNode(node, {0});
  if (delete_ret != SUCCESS) {
    RollbackAddedControlEdges(added_control_edges);
    GELOGE(delete_ret, "[Delete][Node] IsolateAndDeleteNode failed for node %s.", node->GetName().c_str());
    return delete_ret;
  }

  MergeTensorMoveSymbolAfterDelete(node, has_symbol_table_, anchor_to_symbol_, symbol_to_anchors_);

  LogTensorMoveDeleted(node, added_control_edges, added_control_edge_descs);
  return SUCCESS;
}

REG_PASS_OPTION("TensorMoveDeletePass").LEVELS(OoLevel::kO3);
}  // namespace ge
