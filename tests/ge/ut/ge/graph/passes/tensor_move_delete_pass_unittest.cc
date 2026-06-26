/* Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * ===================================================================================================================*/

#include <gtest/gtest.h>

#include "macro_utils/dt_public_scope.h"
#include "common/op/ge_op_utils.h"
#include "common/framework_types_internal.h"
#include "graph/anchor.h"
#include "graph/compute_graph.h"
#include "graph/op_desc.h"
#include "graph/passes/base_pass.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/tensor_utils.h"
#include "graph_builder_utils.h"
#include "ge_local_context.h"
#include "graph/passes/standard_optimize/tensor_move_delete_pass.h"
#include "ge_graph_dsl/op_desc/op_desc_cfg_box.h"
#include "easy_graph/builder/graph_dsl.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/operator_reg.h"
#include "external/ge_common/ge_common_api_types.h"
#include "stub/gert_runtime_stub.h"
#include "graph/optimize/mem_rw_conflict_optimize.h"
#include "graph/optimize/mem_layout_conflict_optimize/mem_layout_conflict_util.h"

using namespace std;
using namespace testing;
using namespace ge;

namespace {
REG_OP(Cast)
    .INPUT(x, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64,
                          DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16,
                          DT_QUINT16, DT_QINT32})) /* input tensor */
    .OUTPUT(y, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8, DT_INT64, DT_UINT64,
                           DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16,
                           DT_QUINT16, DT_QINT32})) /* output tensor */
    .ATTR(dst_type, Int, 0)
    .ATTR(truncate, Bool, false)
    .OP_END_FACTORY_REG(Cast)

        bool SetTransDataTensorDesc(const ComputeGraphPtr &root_graph, const std::vector<std::string> &node_names,
                                    Format format = FORMAT_NCL) {
  GeTensorDesc tensor_desc{GeShape{{2022, 2023}}, format, DT_FLOAT16};
  std::map<std::string, NodePtr> all_transdata_map;
  for (auto &node : root_graph->GetAllNodes()) {
    if (node->GetType() == TRANSDATA) {
      all_transdata_map[node->GetName()] = node;
    }
  }
  for (const auto &node_name : node_names) {
    const auto iter = all_transdata_map.find(node_name);
    if (iter != all_transdata_map.end()) {
      iter->second->GetOpDesc()->UpdateOutputDesc(0, tensor_desc);
    } else {
      std::cout << "========================================" << std::endl;
      std::cout << "cannot find " << node_name << std::endl;
      std::cout << "========================================" << std::endl;
      return false;
    }
  }
  return true;
}

using NetoutputParentIndexes = std::vector<std::pair<std::string, std::vector<uint32_t>>>;
bool AddParentIndexForNetoutput(ComputeGraphPtr &root_graph, NetoutputParentIndexes &indexes) {
  std::map<std::string, NodePtr> netoutput_map;
  for (auto &node : root_graph->GetAllNodes()) {
    netoutput_map[node->GetName()] = node;
  }
  for (auto &name_indexes_pair : indexes) {
    const auto iter = netoutput_map.find(name_indexes_pair.first);
    if (iter == netoutput_map.end()) {
      std::cout << "========================================" << std::endl;
      std::cout << "cannot find " << name_indexes_pair.first << std::endl;
      std::cout << "========================================" << std::endl;
      return false;
    }
    auto op_desc = iter->second->GetOpDesc();
    size_t input_index = 0U;
    if (name_indexes_pair.second.size() != op_desc->GetInputsSize()) {
      std::cout << "========================================" << std::endl;
      std::cout << name_indexes_pair.first << " real inputs size: " << op_desc->GetInputsSize()
                << ", but name_indexes_pair.second.size(): " << name_indexes_pair.second.size() << std::endl;
      std::cout << "========================================" << std::endl;
      return false;
    }
    for (auto parent_index : name_indexes_pair.second) {
      auto tensor_desc = op_desc->MutableInputDesc(input_index++);
      AttrUtils::SetInt(tensor_desc, ATTR_NAME_PARENT_NODE_INDEX, parent_index);
    }
  }
  return true;
}

void SetRefOutput(const NodePtr &node, const uint32_t output_idx = 0U, const int32_t input_idx = 0) {
  auto out_desc = node->GetOpDescBarePtr()->MutableOutputDesc(output_idx);
  ge::TensorUtils::SetReuseInput(*out_desc, true);
  ge::TensorUtils::SetReuseInputIndex(*out_desc, input_idx);
}

void SetModifyInput(const NodePtr &node, const bool value = true) {
  AttrUtils::SetBool(node->GetOpDesc(), "_input_mutable", value);
}
}  // namespace

class UtestTensorMoveDeletePass : public Test {
 protected:
  void SetUp() {
    dlog_setlevel(0, 0, 0);
  }

  void TearDown() {
    dlog_setlevel(0, 3, 0);
    unsetenv("DUMP_GRAPH_LEVEL");
    unsetenv("DUMP_GE_GRAPH");
    GetThreadLocalContext().SetGraphOption({});
  }
};

/**
 *       Relu
 *        |
 *     TensorMove
 *        |
 *     NetOutput
 *
 * 说明：
 * - TensorMove 前后均为普通节点
 * - Relu无分支
 *
 * 预期：
 * - 删除 TensorMove，Relu 直连 NetOutput
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNodeToNetOutput_Deleted) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *         Relu
 *        /    \
 *      Add   TensorMove
 *       |       |
 *       +---- NetOutput
 *
 * 说明：
 * - Relu 单输出被 Add 和 TensorMove 同时引用
 * - Add 为普通读分支，不透传也不写输入
 * - TensorMove 后继 NetOutput 也是纯读
 *
 * 预期：
 * - 删除 TensorMove
 * - Relu 直连 NetOutput
 * - 旁路与后继均为纯读，二者间无读写冒险，不新增 Add 到 NetOutput 的控制边
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_WithBasicMultiRefBranch_DeletedWithoutControlEdge) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_TRUE(relu_node->GetOutDataAnchor(0)->IsLinkedWith(netoutput_node->GetInDataAnchor(0)));
  EXPECT_FALSE(add_node->GetOutControlAnchor()->IsLinkedWith(netoutput_node->GetInControlAnchor()));
}

/**
 *            Relu
 *          /      \
 * PartitionedCall TensorMove
 *       |             |
 *       +---------- NetOutput
 *
 * 说明：
 * - Relu 单输出被 PartitionedCall 和 TensorMove 同时引用
 * - PartitionedCall 是带子图的 wrapper 节点，真实读写发生在子图内部
 *
 * 预期：
 * - 保留 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_WithWrapperSibling_Kept) {
  DEF_GRAPH(sub) {
    CHAIN(NODE("sub_data", OP_CFG(DATA).ParentNodeIndex(0))->NODE("sub_netoutput", NETOUTPUT));
  };

  DEF_GRAPH(g1) {
    CHAIN(NODE("Relu", RELU)->NODE("TensorMove", TENSORMOVE)->EDGE(0, 0)->NODE("NetOutput", NETOUTPUT));
    CHAIN(NODE("Relu")->EDGE(0, 0)->NODE("PartitionedCall", PARTITIONEDCALL, sub)->EDGE(0, 1)->NODE("NetOutput"));
  };

  auto graph = ToComputeGraph(g1);
  ASSERT_NE(graph, nullptr);
  ASSERT_NE(graph->FindNode("PartitionedCall"), nullptr);

  ge::GEPass pass(graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(graph->FindNode("TensorMove"), nullptr);
}

/**
 *         Relu
 *        /    \
 *      Add   TensorMove
 *       |       |
 *       |     Succ(modify_input)
 *       +-------+
 *
 * 说明：
 * - Relu 单输出被纯读旁路 Add 和 TensorMove 同时引用
 * - TensorMove 后继 Succ 的输入读写关系为 kScopeWriteable
 * - 删除后 Relu(out) 到 Succ(in) 会产生 RW 冲突
 *
 * 预期：
 * - 保留 TensorMove
 * - 不落地 Add 到 Succ 的 pending 控制边
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_SuccessorModifyInput_KeptByRWConflict) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);

  SetModifyInput(succ_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_TRUE(tensor_move_node->GetOutDataAnchor(0)->IsLinkedWith(succ_node->GetInDataAnchor(0)));
  EXPECT_FALSE(relu_node->GetOutDataAnchor(0)->IsLinkedWith(succ_node->GetInDataAnchor(0)));
  EXPECT_FALSE(add_node->GetOutControlAnchor()->IsLinkedWith(succ_node->GetInControlAnchor()));
}

/**
 *         Relu
 *        /    \
 *      Add   RefOp1
 *       |      |
 *       |   TensorMove
 *       |      |
 *       |    RefOp2
 *       |      |
 *       +--- NetOutput
 *
 * 说明：
 * - Relu 单输出被 Add 和 RefOp1 同时引用
 * - TensorMove 不是 Relu 的直接消费者，而是挂在输出复用输入算子 RefOp1 之后
 * - 当前 basic multi-ref 规则不证明 RefOp1 -> TensorMove 这段间接链的完整生命周期
 *
 * 预期：
 * - 不删除 TensorMove
 * - 不添加 Add 到 RefOp2 的控制边
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveBehindRefConsumer_WithBasicMultiRefBranch_Kept) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto ref1_node = builder.AddNode("RefOp1", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto ref2_node = builder.AddNode("RefOp2", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetRefOutput(ref1_node, 0, 0);
  SetRefOutput(ref2_node, 0, 0);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), ref1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref1_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), ref2_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref2_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_FALSE(add_node->GetOutControlAnchor()->IsLinkedWith(ref2_node->GetInControlAnchor()));
}

/**
 *        Relu
 *       /    \
 *      |   TensorMove
 *       \    /
 *      ConsumerB
 *         |
 *      NetOutput
 *
 * 说明：
 * - ConsumerB 同时作为旁路消费者（读 in0）和 TensorMove 后继（读 in1）
 * - ConsumerB 的输入读写关系为 kScopeWriteable，删除后同一节点会同时作为旁路写者和 TM 后继写者
 *
 * 预期：
 * - 不删除 TensorMove
 * - 不添加 ConsumerB 到自身的控制边
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_SiblingEqualsSuccessor_Kept) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto consumer_node = builder.AddNode("ConsumerB", ADD, 2, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  SetModifyInput(consumer_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), consumer_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), consumer_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(consumer_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_FALSE(consumer_node->GetOutControlAnchor()->IsLinkedWith(consumer_node->GetInControlAnchor()));
}

/**
 *        Relu
 *       /    \
 *  TensorMove Sibling
 *      |       ^
 *    Succ -----|
 *
 * 说明：
 * - Sibling 是 Relu 的旁路消费者
 * - TensorMove 后继 Succ 的输入读写关系为 kScopeWriteable，删 TM 后需补 Sibling -> Succ 保序
 * - 但 Succ 已经通过数据边到达 Sibling，再补 Sibling -> Succ 控制边会形成环
 *
 * 预期：
 * - 不删除 TensorMove
 * - 不添加 Sibling 到 Succ 的控制边
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_SuccessorReachesSiblingViaDataPath_Kept) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);
  auto sibling_node = builder.AddNode("Sibling", ADD, 2, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  SetModifyInput(succ_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(succ_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(sibling_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_FALSE(sibling_node->GetOutControlAnchor()->IsLinkedWith(succ_node->GetInControlAnchor()));
}

/**
 *           Relu
 *      /      |      \
 *   Add0     Add1   TensorMove
 *     |        |         |
 *     +--------+----- NetOutput
 *
 * 说明：
 * - Relu 单输出被两个普通读分支和一个 TensorMove 同时引用
 * - TensorMove 后继 NetOutput 也是纯读
 *
 * 预期：
 * - 删除 TensorMove
 * - Relu 直连 NetOutput
 * - 旁路与后继均为纯读，不向 NetOutput 补控制边
 */
TEST_F(UtestTensorMoveDeletePass,
       TensorMoveFromComputeNode_WithMultipleBasicMultiRefBranches_DeletedWithoutControlEdges) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add0_node = builder.AddNode("Add0", ADD, 1, 1);
  auto add1_node = builder.AddNode("Add1", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 3, 1);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add0_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add0_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add1_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(2));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_TRUE(relu_node->GetOutDataAnchor(0)->IsLinkedWith(netoutput_node->GetInDataAnchor(0)));
  EXPECT_FALSE(add0_node->GetOutControlAnchor()->IsLinkedWith(netoutput_node->GetInControlAnchor()));
  EXPECT_FALSE(add1_node->GetOutControlAnchor()->IsLinkedWith(netoutput_node->GetInControlAnchor()));
}

/**
 *        Data
 *         |
 *  TensorMove0(保留)
 *         |
 *  ScatterNDUpdate0
 *      /         \
 * MlpLightningIndexer0 TensorMove1
 *         |             |
 * SparseFlashAttention0 ScatterNDUpdate1
 *         |             |
 *         +------ NetOutput ---- MlpLightningIndexer1
 *
 * 说明：
 * - 待删节点是 TensorMove1
 * - 分叉点位于 TensorMove1 的直接源节点 ScatterNDUpdate0，不是根 Data
 * - 上游 TensorMove0 通过保留属性固定不删除
 * - TensorMove1 后继 ScatterNDUpdate1 是纯读
 *
 * 预期：
 * - 删除 TensorMove1
 * - ScatterNDUpdate0 直连 ScatterNDUpdate1
 * - 旁路 MlpLightningIndexer0 与后继 ScatterNDUpdate1 均为纯读，不新增控制边
 * - TensorMove0 保留
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromIntermediateSource_WithScatterBranch_DeletedWithoutControlEdge) {
  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto tensor_move0_node = builder.AddNode("TensorMove0", TENSORMOVE, 1, 1);
  auto scatter0_node = builder.AddNode("ScatterNDUpdate0", RELU, 1, 1);
  auto mlp_indexer0_node = builder.AddNode("MlpLightningIndexer0", RELU, 1, 1);
  auto sfa0_node = builder.AddNode("SparseFlashAttention0", RELU, 1, 1);
  auto tensor_move1_node = builder.AddNode("TensorMove1", TENSORMOVE, 1, 1);
  auto scatter1_node = builder.AddNode("ScatterNDUpdate1", RELU, 1, 1);
  auto mlp_indexer1_node = builder.AddNode("MlpLightningIndexer1", RELU, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 3, 1);

  AttrUtils::SetBool(tensor_move0_node->GetOpDesc(), ATTR_NAME_CANNOT_BE_DELETED, true);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), tensor_move0_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move0_node->GetOutDataAnchor(0), scatter0_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(scatter0_node->GetOutDataAnchor(0), mlp_indexer0_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(mlp_indexer0_node->GetOutDataAnchor(0), sfa0_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(scatter0_node->GetOutDataAnchor(0), tensor_move1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move1_node->GetOutDataAnchor(0), scatter1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(scatter1_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(sfa0_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(scatter1_node->GetOutDataAnchor(0), mlp_indexer1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(mlp_indexer1_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(2));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove0"), nullptr);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove1"), nullptr);
  EXPECT_TRUE(scatter0_node->GetOutDataAnchor(0)->IsLinkedWith(scatter1_node->GetInDataAnchor(0)));
  EXPECT_FALSE(mlp_indexer0_node->GetOutControlAnchor()->IsLinkedWith(scatter1_node->GetInControlAnchor()));
}

/**
 *         Relu
 *        /    \
 *   Add(modify_input) TensorMove
 *        |          |
 *        +------ NetOutput
 *
 * 说明：
 * - Relu 单输出被 Add 和 TensorMove 同时引用
 * - Add 的输入读写关系为 kScopeWriteable
 *
 * 预期：
 * - 不删除 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_WithWritableMultiRefBranch_Kept) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetModifyInput(add_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_FALSE(add_node->GetOutControlAnchor()->IsLinkedWith(netoutput_node->GetInControlAnchor()));
}

/**
 *          Relu
 *         /    \
 * Add(modify_input) TensorMove
 *        ^        |
 *        +----- RefSucc
 *                 |
 *              NetOutput
 *
 * 说明：
 * - 旁路 Add 的输入读写关系为 kScopeWriteable，会覆写 Relu 输出对应的源内存
 * - TensorMove 后继 RefSucc 输出复用输入，源内存生命周期会继续传给 NetOutput
 * - 即使存在 RefSucc -> Add 控制边，也只能保证 RefSucc 自己先执行，不能保证 RefSucc 下游读完
 *
 * 预期：
 * - 不删除 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromComputeNode_SuccessorRefOutputWithOverwriteSibling_Kept) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto ref_succ_node = builder.AddNode("RefSucc", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetModifyInput(add_node);
  SetRefOutput(ref_succ_node, 0, 0);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), ref_succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_succ_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_succ_node->GetOutControlAnchor(), add_node->GetInControlAnchor());

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *       Data
 *        |
 *     TensorMove
 *        |
 *     ReShape1 (refop)
 *        |
 *     ReShape2 (refop)
 *        |
 *     NetOutput
 *
 * 说明：
 * - TensorMove 位于 Data 与 reshape之间
 * - 开启输出复用输入内存
 *
 * 预期：
 * - 删除 TensorMove，Data 直连 ReShape1
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromDataToNetOutput_ThroughRefOps_Deleted) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("ReShape1", RESHAPE, 1, 1);
  auto node22 = builder.AddNode("ReShape2", RESHAPE, 1, 1);
  auto node3 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(relu_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));
  GraphUtils::AddEdge(node2->GetOutDataAnchor(0), node22->GetInDataAnchor(0));
  GraphUtils::AddEdge(node22->GetOutDataAnchor(0), node3->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *       Data
 *        |
 *     ReShape (refop)
 *        |
 *     TensorMove
 *        |
 *     NetOutput(复用内存)
 *
 * 说明：
 * - TensorMove 位于 ReShape 与 NetOutput 之间，实际输入为根图Data
 * - 单输入单输出，无分支，复用内存
 *
 * 预期：
 * - 删除 TensorMove，ReShape 直连 NetOutput
 */

TEST_F(UtestTensorMoveDeletePass, TensorMoveFromDataToNetOutput_ThroughSingleRefOp_Deleted) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto reshape_node = builder.AddNode("ReShape", RESHAPE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), reshape_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(reshape_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  EXPECT_EQ(tensor_move_node->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *       Data
 *        |
 *      Reshape -- Add
 *        |
 *     TensorMove
 *        |
 *     NetOutput
 *
 * 说明：
 * - TensorMove的实际源输入为根图Data，Reshape 的输出同时供给 Add 节点和 TensorMove
 *
 * 预期：
 * - 不删除 TensorMove，图结构不变
 *
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromDataViaRefOp_WithBranch_Kept) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  // 创建算子：1个输入，2个输出
  auto ref_node2 = builder.AddNode("ref_node2", ADD, 1, 2);
  auto op_desc = ref_node2->GetOpDescBarePtr();
  auto out_desc_0 = op_desc->MutableOutputDesc(0);
  auto out_desc_1 = op_desc->MutableOutputDesc(1);
  ge::TensorUtils::SetReuseInput(*out_desc_0, true);
  ge::TensorUtils::SetReuseInputIndex(*out_desc_0, 0);  // 复用第0个输入
  ge::TensorUtils::SetReuseInput(*out_desc_1, true);
  ge::TensorUtils::SetReuseInputIndex(*out_desc_1, 0);  // 同样复用第0个输入
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), ref_node2->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_node2->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_node2->GetOutDataAnchor(1), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  EXPECT_EQ(tensor_move_node->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *        Data
 *       /   \
 * TensorMove1 Add
 *      |
 *    RefOp
 *      |
 * TensorMove2
 *      |
 *   NetOutput
 *
 * 说明：
 * - Data 被 TensorMove1 和 Add 同时使用，当前基础多消费者规则可通过补控制边删除 TensorMove1
 * - TensorMove2 向上溯源经过 RefOp 后遇到 TensorMove1，停止穿透
 *
 * 预期：
 * - TensorMove1 删除
 * - TensorMove2 删除
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveTraceStopsAtUpstreamTensorMove_DataBranched_DownstreamDeleted) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);
  dlog_setlevel(0, 0, 0);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto tensor_move1_node = builder.AddNode("TensorMove1", TENSORMOVE, 1, 1);
  auto ref_node = builder.AddNode("RefOp", ADD, 1, 1);
  auto tensor_move2_node = builder.AddNode("TensorMove2", TENSORMOVE, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetRefOutput(ref_node, 0, 0);
  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), tensor_move1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move1_node->GetOutDataAnchor(0), ref_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_node->GetOutDataAnchor(0), tensor_move2_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move2_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove1"), nullptr);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove2"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
  dlog_setlevel(0, 3, 0);
}

/**
 *         Data
 *          |
 *   TensorMove1(reserved)
 *      /         \
 *   RefOp        Add
 *    |            |
 * TensorMove2  NetOutput
 *    |
 * NetOutput
 *
 * 说明：
 * - TensorMove1 通过保留属性禁止删除
 * - TensorMove1 输出有分支（RefOp 与 Add）
 * - TensorMove2 溯源遇到 TensorMove1 停止，随后按单路径规则校验失败
 *
 * 预期：
 * - TensorMove1 保留
 * - TensorMove2 保留
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveTraceStopsAtReservedUpstreamTensorMove_WithBranch_DownstreamKept) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto tensor_move1_node = builder.AddNode("TensorMove1", TENSORMOVE, 1, 1);
  auto ref_node = builder.AddNode("RefOp", ADD, 1, 1);
  auto tensor_move2_node = builder.AddNode("TensorMove2", TENSORMOVE, 1, 1);
  auto add_node = builder.AddNode("Add", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetRefOutput(ref_node, 0, 0);
  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);
  AttrUtils::SetBool(tensor_move1_node->GetOpDesc(), ATTR_NAME_CANNOT_BE_DELETED, true);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), tensor_move1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move1_node->GetOutDataAnchor(0), ref_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_node->GetOutDataAnchor(0), tensor_move2_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move2_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move1_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove1"), nullptr);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove2"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *       Data
 *        |
 *     TensorMove
 *        |
 *     NetOutput
 *
 * 说明：
 * - TensorMove 输入为根图Data，直连NetOutput
 * - 未设置地址复用
 *
 * 预期行为：
 * - 根图Data为外部输入，未声明输出复用输入内存，不删除TensorMove
 */
TEST_F(UtestTensorMoveDeletePass,
       TensorMoveFromData_NoReuseConfig_Kept) {  // 还能补充一个：配置了option，但是不包含0，0
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *       Data
 *        |
 *     TensorMove
 *        |
 *     NetOutput  (复用输入地址)
 *
 * 说明：
 * - TensorMove 输入为根图Data，直连NetOutput
 * - 设置了输出复用输入内存
 * - 单输入、单输出
 * - 无分支、无子图
 *
 * 预期行为：
 * - 删除TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromData_MemReuse_Deleted) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 原始模型：
 *
 *         Relu
 *        /    \
 *   TensorMove  TransData
 *        |
 *     NetOutput
 *
 * 说明：
 * - TensorMove的输入为Relu，直连NetOutput
 * - Relu还有另一输出连接到 TransData
 *
 * 预期行为：
 * - Relu输出给到多个节点，不删除TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromRelu_SourceHasMultiOutputs_Kept) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  auto builder = ut::GraphBuilder("g1");
  // 创建算子：1个输入，2个输出
  auto ref_node2 = builder.AddNode("ref_node2", ADD, 1, 2);
  auto op_desc = ref_node2->GetOpDescBarePtr();
  auto out_desc_0 = op_desc->MutableOutputDesc(0);
  auto out_desc_1 = op_desc->MutableOutputDesc(1);
  ge::TensorUtils::SetReuseInput(*out_desc_0, true);
  ge::TensorUtils::SetReuseInputIndex(*out_desc_0, 0);  // 复用第0个输入
  ge::TensorUtils::SetReuseInput(*out_desc_1, true);
  ge::TensorUtils::SetReuseInputIndex(*out_desc_1, 0);  // 同样复用第0个输入
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);
  auto node3 = builder.AddNode("Transdata", TRANSDATA, 1, 1);

  GraphUtils::AddEdge(ref_node2->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(ref_node2->GetOutDataAnchor(1), node3->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 父图:                子图 sub_1:
 * Data                 sub_Data (ParentIndex: 0)
 * |                      |
 * PartitionedCall ------> TensorMove
 * |                      |
 * NetOutput            sub_NetOutput
 * (复用输入地址)
 *
 * 场景说明：
 * - 子图内部 TensorMove 的前驱是 sub_Data，其在父图的实际源头是 Data。
 * - 设置根图 NetOutput 复用输入内存，触发 TensorMove 优化逻辑。
 *
 * 预期行为：
 * - Trace 能够跨越子图边界识别到 Data 是源头。
 * - TensorMove 被成功识别并删除。
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveInSubgraph_FromParentData_Deleted) {
  // 1. 设置内存复用选项：设置根图的第 0 个输出复用第 0 个输入
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  // 2. 构造子图 sub_1
  // sub_Data 的 ParentNodeIndex(0) 代表它对应父图中 PartitionedCall 的第 0 个 Input
  const auto sub_data_cfg = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data_cfg)->NODE("sub_tensormove", TENSORMOVE)->NODE("sub_netoutput", NETOUTPUT));
  };

  // 3. 构造父图 g1
  const auto data_cfg = OP_CFG(DATA).Attr(ATTR_NAME_INDEX, 0);
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", data_cfg)
              ->EDGE(0, 0)
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("netoutput", NETOUTPUT));
  };

  // 4. 将子图挂载到父图
  auto compute_graph = ToComputeGraph(g1);
  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);

  auto p_call_node = compute_graph->FindNode("partitioned_call");
  ASSERT_NE(p_call_node, nullptr);

  // 设置父子图关联属性
  sub_graph_1->SetParentGraph(compute_graph);
  sub_graph_1->SetParentNode(p_call_node);

  // 5. 设置 NetOutput 的映射关系 (用于 Trace 穿透)
  // 根图 NetOutput 的输入来自 PartitionedCall:0
  // 子图 sub_NetOutput 的输入来自 sub_tensormove
  NetoutputParentIndexes indexes{{"netoutput", {0}}, {"sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  // 6. 执行 Pass
  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  // 7. 验证结果：子图内部的 tensormove 应该被删除
  // 注意：FindNode 在子图中查找
  EXPECT_EQ(sub_graph_1->FindNode("sub_tensormove"), nullptr);

  // 清理环境
  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 父图:
 * Data -> PartitionedCall -> NetOutput
 *
 * 子图 sub_1:
 * sub_Data(ParentIndex:0) -> sub_tensormove -> sub_NetOutput
 *
 * 场景说明：
 * - sub_tensormove 从子图 Data 跳出到父图时，source path 中会插入 PartitionedCall 节点。
 *
 * 预期行为：
 * - trace 路径日志中包含 partitioned_call 节点；
 * - partitioned_call 仅打印节点名，不打印 out anchor；
 * - sub_tensormove 可被删除（保证路径后续规则可正常处理空 anchor）。
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveInSubgraph_PartitionedCallSourcePath_LogWithoutOutAnchor) {
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  const auto sub_data_cfg = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data_cfg)->NODE("sub_tensormove", TENSORMOVE)->NODE("sub_netoutput", NETOUTPUT));
  };

  const auto data_cfg = OP_CFG(DATA).Attr(ATTR_NAME_INDEX, 0);
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", data_cfg)
              ->EDGE(0, 0)
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto compute_graph = ToComputeGraph(g1);
  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);

  auto p_call_node = compute_graph->FindNode("partitioned_call");
  ASSERT_NE(p_call_node, nullptr);
  sub_graph_1->SetParentGraph(compute_graph);
  sub_graph_1->SetParentNode(p_call_node);

  NetoutputParentIndexes indexes{{"netoutput", {0}}, {"sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetSlogStub().SetLevel(DLOG_INFO);
  runtime_stub.GetSlogStub().Clear();

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(sub_graph_1->FindNode("sub_tensormove"), nullptr);
  EXPECT_NE(runtime_stub.GetSlogStub().FindLog(
                DLOG_INFO,
                "Trace reach real source: data(out:0)-->partitioned_call-->sub_data(out:0)-->(in:0)sub_tensormove"),
            -1);
  EXPECT_EQ(runtime_stub.GetSlogStub().FindLogRegex(DLOG_INFO, "Trace reach real source: .*partitioned_call\\(out:"),
            -1);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 主图：
 *         Data
 *        /    \
 *     Cast    PartitionedCall
 *      |        |
 *   TransData  NetOutput
 *
 * 子图 sub_1：
 *      sub_Data
 *          |
 *    sub_partitioned_call
 *          |
 *     TensorMove
 *          |
 *     sub_NetOutput
 *
 * 子子图 sub_sub_1：
 *       sub_sub_data
 *        /       \
 *     Cast        \
 *      |           \
 *   TransData      Add
 *      \            /
 *      Add        /
 *        \      /
 *      sub_sub_NetOutput
 *
 *
 * 预期行为：
 * - 删除
 * TensorMove,sub_sub_NetOutput两个输出，一个空悬，一个给到TensorMove，但是任意一个的输入都是计算节点(TransData或Add)
 */
TEST_F(UtestTensorMoveDeletePass, TensorMove_NestedPCall_FromAdd_Deleted) {
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  const auto sub_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_sub_1) {
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->NODE("sub_sub_cast", CAST)
              ->NODE("sub_sub_transdata", TRANSDATA)
              ->NODE("sub_sub_add0", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->EDGE(0, 0)
              ->NODE("sub_sub_add1", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
  };
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data)
              ->NODE("sub_partitioned_call", PARTITIONEDCALL, sub_sub_1)
              ->NODE("sub_tensor_move", TENSORMOVE)
              ->NODE("sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(
        NODE("data", DATA)->EDGE(0, 0)->NODE("cast", CAST)->NODE("transdata", TRANSDATA)->NODE("netoutput", NETOUTPUT));
    CHAIN(NODE("data", DATA)
              ->EDGE(0, 0)
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("netoutput", NETOUTPUT));
  };
  auto sub_sub_1_graph = ToComputeGraph(sub_sub_1);
  sub_1.Layout();
  auto compute_graph = ToComputeGraph(g1);
  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);

  auto sub_partitioned_call_node = sub_graph_1->FindNode("sub_partitioned_call");
  ASSERT_NE(sub_partitioned_call_node, nullptr);
  sub_sub_1_graph->SetParentGraph(compute_graph);
  sub_sub_1_graph->SetParentNode(sub_partitioned_call_node);
  compute_graph->AddSubGraph(sub_sub_1_graph);  // 嵌套子图

  const auto sub_sub_graph_1 = compute_graph->GetSubgraph("sub_sub_1");
  ASSERT_NE(sub_sub_graph_1, nullptr);

  ASSERT_TRUE(SetTransDataTensorDesc(compute_graph, {"transdata", "sub_sub_transdata"}));

  NetoutputParentIndexes indexes{{"sub_netoutput", {0}}, {"sub_sub_netoutput", {0, 1}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_NE(sub_graph_1->FindNode("sub_tensor_move"), nullptr);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(sub_graph_1->FindNode("sub_tensor_move"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *        data
 *          |
 *  PartitionedCall
 *          |
 *      netoutput
 *
 * 子图 sub_1：
 *
 *      sub_data
 *          |
 *  sub_partitioned_call
 *          |
 *       tensormove
 *          |
 *     sub_netoutput
 *
 * 子子图 sub_sub_1：
 *
 *       sub_sub_data
 *        /       \
 *   sub_sub_cast   sub_sub_add1
 *        |           |
 * sub_sub_transdata  |
 *        |           |
 *   sub_sub_add0 -----
 *        |
 *  sub_sub_netoutput
 *
 * 预期结果：
 * tensormove的输入是sub_sub_add0，sub_sub_add0只有一条路径，删除
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveInSub_FromSubSubAdd_Deleted) {
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  const auto sub_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_sub_1) {
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->NODE("sub_sub_cast", CAST)
              ->NODE("sub_sub_transdata", TRANSDATA)
              ->NODE("sub_sub_add0", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->EDGE(0, 0)
              ->NODE("sub_sub_add1", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
  };
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data)
              ->NODE("sub_partitioned_call", PARTITIONEDCALL, sub_sub_1)
              ->NODE("sub_tensor_move", TENSORMOVE)
              ->NODE("sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->EDGE(0, 0)
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("netoutput", NETOUTPUT));
  };
  auto sub_sub_1_graph = ToComputeGraph(sub_sub_1);
  sub_1.Layout();
  auto compute_graph = ToComputeGraph(g1);
  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);

  auto sub_partitioned_call_node = sub_graph_1->FindNode("sub_partitioned_call");
  ASSERT_NE(sub_partitioned_call_node, nullptr);
  sub_sub_1_graph->SetParentGraph(compute_graph);
  sub_sub_1_graph->SetParentNode(sub_partitioned_call_node);
  compute_graph->AddSubGraph(sub_sub_1_graph);  // 嵌套子图

  const auto sub_sub_graph_1 = compute_graph->GetSubgraph("sub_sub_1");
  ASSERT_NE(sub_sub_graph_1, nullptr);

  ASSERT_TRUE(SetTransDataTensorDesc(compute_graph, {"sub_sub_transdata"}));

  NetoutputParentIndexes indexes{{"sub_netoutput", {0}}, {"sub_sub_netoutput", {0, 1}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_NE(sub_graph_1->FindNode("sub_tensor_move"), nullptr);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(sub_graph_1->FindNode("sub_tensor_move"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 主图 g1：
 *
 *        data
 *          |
 *   PartitionedCall
 *          |
 *       tensormove
 *          |
 *       netoutput
 *
 * 子图 sub_1：
 *
 *      sub_data
 *          |
 *  sub_partitioned_call
 *          |
 *     sub_tensormove
 *          |
 *     sub_netoutput
 *
 * 子子图 sub_sub_1：
 *
 *       sub_sub_data
 *        /       \
 *   sub_sub_cast   sub_sub_add1
 *        |           |
 * sub_sub_transdata  |
 *        |           |
 *   sub_sub_add0     |
 *        |
 *     sub_sub_netoutput

 *
 * 预期结果：
 * - 主图 tensormove 的真实输入应追溯至子子图的 sub_sub_add0，tensormove 被成功删除；
 * - sub_tensormove 的真实输入应追溯至子子图中的 sub_sub_add0，sub_tensormove 也被成功删除；
 */

TEST_F(UtestTensorMoveDeletePass, TensorMoveInRootAndSub_FromSubSubAdd_Deleted) {
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  const auto sub_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_sub_1) {
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->NODE("sub_sub_cast", CAST)
              ->NODE("sub_sub_transdata", TRANSDATA)
              ->NODE("sub_sub_add0", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("sub_sub_data", sub_sub_data)
              ->EDGE(0, 0)
              ->NODE("sub_sub_add1", ADD)
              ->NODE("sub_sub_netoutput", NETOUTPUT));
  };
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data)
              ->NODE("sub_partitioned_call", PARTITIONEDCALL, sub_sub_1)
              ->NODE("sub_tensormove", TENSORMOVE)
              ->NODE("sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->EDGE(0, 0)
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("tensormove", TENSORMOVE)
              ->EDGE(0, 0)
              ->NODE("netoutput", NETOUTPUT));
  };
  auto sub_sub_1_graph = ToComputeGraph(sub_sub_1);
  sub_1.Layout();
  auto compute_graph = ToComputeGraph(g1);
  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);

  auto sub_partitioned_call_node = sub_graph_1->FindNode("sub_partitioned_call");
  ASSERT_NE(sub_partitioned_call_node, nullptr);
  sub_sub_1_graph->SetParentGraph(compute_graph);
  sub_sub_1_graph->SetParentNode(sub_partitioned_call_node);
  sub_sub_1_graph->SetOutputSize(2);
  compute_graph->AddSubGraph(sub_sub_1_graph);  // 嵌套子图

  const auto sub_sub_graph_1 = compute_graph->GetSubgraph("sub_sub_1");
  ASSERT_NE(sub_sub_graph_1, nullptr);

  ASSERT_TRUE(SetTransDataTensorDesc(compute_graph, {"sub_sub_transdata"}));

  NetoutputParentIndexes indexes{{"sub_netoutput", {0}}, {"sub_sub_netoutput", {0, 1}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(compute_graph->FindNode("tensormove"), nullptr);
  EXPECT_EQ(compute_graph->FindNode("sub_tensormove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 主图 g1：
 *
 *   PartitionedCall_1
 *          |
 *   PartitionedCall_2
 *          |
 *         relu
 *          |
 *       netoutput
 *
 * 子图 sub_1：
 *     sub_1_constant
 *          |
 *     sub_1_netoutput
 *
 * 子图 sub_2：
 *      sub_2_data
 *          |
 *     sub_2_tensormove
 *          |
 *     sub_2_netoutput
 *
 *
 * 预期结果：
 * - sub_2_tensormove的真实输入应追溯至sub_1_constant，sub_2_tensormove 不能删除；
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveInSub2_TraceToSub1Const_NotDeleted) {
  // 1. 定义 sub_1： Const -> NetOutput
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_1_constant", CONSTANT)->NODE("sub_1_netoutput", NETOUTPUT));
  };

  // 2. 定义 sub_2： Data -> TensorMove -> NetOutput
  // ParentNodeIndex(0) 表示 sub_2_data 对应主图 wrapper 节点的第0个输入
  const auto sub_2_data_cfg = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_2) {
    CHAIN(NODE("sub_2_data", sub_2_data_cfg)->NODE("sub_2_tensormove", TENSORMOVE)->NODE("sub_2_netoutput", NETOUTPUT));
  };

  // 3. 定义主图 g1 结构: PartitionedCall_1 -> PartitionedCall_2 -> Relu -> NetOutput
  DEF_GRAPH(g1) {
    CHAIN(NODE("PartitionedCall_1", PARTITIONEDCALL, sub_1)
              ->EDGE(0, 0)
              ->NODE("PartitionedCall_2", PARTITIONEDCALL, sub_2)
              ->NODE("relu", RELU)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto sub_1_graph = ToComputeGraph(sub_1);
  auto sub_2_graph = ToComputeGraph(sub_2);
  auto compute_graph = ToComputeGraph(g1);

  const auto sub_graph_1 = compute_graph->GetSubgraph("sub_1");
  ASSERT_NE(sub_graph_1, nullptr);
  const auto sub_graph_2 = compute_graph->GetSubgraph("sub_2");
  ASSERT_NE(sub_graph_2, nullptr);

  NetoutputParentIndexes indexes{
      {"sub_1_netoutput", {0}},  // sub_1_netoutput 的 input:0 对应 PartitionedCall_1 的 output:0
      {"sub_2_netoutput", {0}}   // sub_2_netoutput 的 input:0 对应 PartitionedCall_2 的 output:0
  };
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  GE_DUMP(compute_graph, "GraphBefore_SiblingTrace");

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  // 预期：sub_2_tensormove 应该依然存在 (未被删除)
  EXPECT_NE(sub_2_graph->FindNode("sub_2_tensormove"), nullptr);

  GE_DUMP(compute_graph, "GraphAfter_SiblingTrace");
}

/**
 * 主图 g1：
 *
 *        data
 *          |
 *        relu
 *          |
 *          IF
 *          |
 *      transdata
 *          |
 *      tensormove
 *          |
 *      netoutput
 *
 * if 分支子图 if_sub：
 *
 *        if_sub_data
 *           |\
 *           | if_transdata
 *           |     |
 *           |  if_tensormove
 *           |     |
 *           |   if_relu
 *           |     |
 *           ----if_sub_netoutput
 *
 * then 分支子图 then_sub：
 *
 *      then_sub_data
 *           |
 *       then_relu
 *           |
 *     then_sub_netoutput
 *
 * 预期行为：
 * - tensormove的输入是transdata，只有一条路径，被删除
 * - if_tensormove的输入是if_transdata，只有一条路径，被删除
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveInRootAndIfSub_ViaTransData_Deleted) {
  const auto if_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(if_sub) {
    CHAIN(NODE("if_sub_data", if_sub_data)->EDGE(0, 0)->NODE("if_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("if_sub_data", if_sub_data)
              ->EDGE(0, 0)
              ->NODE("if_transdata", TRANSDATA)
              ->NODE("if_tensormove", TENSORMOVE)
              ->NODE("if_relu", RELU)
              ->Ctrl()
              ->NODE("if_sub_netoutput", NETOUTPUT));
  };
  const auto then_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(then_sub) {
    CHAIN(NODE("then_sub_data", then_sub_data)->NODE("then_relu", RELU)->NODE("then_sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("relu", RELU)
              ->NODE("if", IF, if_sub, then_sub)
              ->NODE("transdata", TRANSDATA)
              ->NODE("tensormove", TENSORMOVE)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto compute_graph = ToComputeGraph(g1);
  const auto then_sub_graph = compute_graph->GetSubgraph("then_sub");
  ASSERT_NE(then_sub_graph, nullptr);
  const auto if_sub_graph = compute_graph->GetSubgraph("if_sub");
  ASSERT_NE(if_sub_graph, nullptr);

  compute_graph->TopologicalSorting();

  NetoutputParentIndexes indexes{{"if_sub_netoutput", {0}}, {"then_sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_NE(compute_graph->FindNode("tensormove"), nullptr);
  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(compute_graph->FindNode("tensormove"), nullptr);
  EXPECT_EQ(if_sub_graph->FindNode("if_tensormove"), nullptr);
}

/**
 * 主图 g1：
 *
 *        data
 *          |
 *        relu
 *          |
 *          IF
 *          |
 *     transdata
 *          |
 *     tensormove
 *          |
 *     netoutput
 *
 *
 * if 分支子图 if_sub：
 *                if_sub_data
 *                 /       \
 *                /         \
 *   if_sub_netoutput     if_tensormove
 *                             |
 *                           if_relu
 *                             |
 *                     if_sub_netoutput
 *
 * then 分支子图 then_sub：
 *
 *        then_sub_data
 *              |
 *          then_relu
 *              |
 *      then_sub_netoutput
 *
 * 预期行为：
 * - if_sub_graph 中的 if_tensormove 保留，上游源节点为 if_sub_data，但 if_sub_data 存在多条输出路径
 * - 主图中的 tensormove 被成功删除，上游源节点为 transdata，transdata → tensormove → netoutput
 */
TEST_F(UtestTensorMoveDeletePass, TensorMove_RootDeleted_SubKept_DueToSourceBranching) {
  const auto if_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(if_sub) {
    CHAIN(NODE("if_sub_data", if_sub_data)->EDGE(0, 0)->NODE("if_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("if_sub_data", if_sub_data)
              ->EDGE(0, 0)
              ->NODE("if_tensormove", TENSORMOVE)
              ->EDGE(0, 0)
              ->NODE("if_relu", RELU)
              ->EDGE(0, 0)
              ->NODE("if_sub_netoutput", NETOUTPUT));
  };
  const auto then_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(then_sub) {
    CHAIN(NODE("then_sub_data", then_sub_data)->NODE("then_relu", RELU)->NODE("then_sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("relu", RELU)
              ->NODE("if", IF, if_sub, then_sub)
              ->NODE("transdata", TRANSDATA)
              ->NODE("tensormove", TENSORMOVE)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto compute_graph = ToComputeGraph(g1);
  const auto then_sub_graph = compute_graph->GetSubgraph("then_sub");
  ASSERT_NE(then_sub_graph, nullptr);
  const auto if_sub_graph = compute_graph->GetSubgraph("if_sub");
  ASSERT_NE(if_sub_graph, nullptr);

  compute_graph->TopologicalSorting();

  NetoutputParentIndexes indexes{{"if_sub_netoutput", {0}}, {"then_sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_NE(compute_graph->FindNode("tensormove"), nullptr);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_EQ(compute_graph->FindNode("tensormove"), nullptr);
}

/**
 * 主图 g1：
 *
 *        data
 *          |
 *        relu
 *          |
 *          IF
 *          |
 *     transdata
 *          |
 *     tensormove
 *          |
 *     netoutput
 *
 *
 * if 分支子图 if_sub：
 *        if_sub_data
 *             |
 *        if_tensormove
 *             |
 *     if_sub_netoutput
 *
 * then 分支子图 then_sub：
 *        then_sub_data
 *              |
 *          then_relu
 *              |
 *      then_sub_netoutput
 *
 * 预期行为：
 * - if_sub_graph 中的 if_tensormove 保留，其源输入为主图中的 relu，但 relu 的下游节点是 IF 控制流算子
 * - 主图中的 tensormove 被成功删除，其源输入为 transdata，输出是netoutput
 */
TEST_F(UtestTensorMoveDeletePass, TensorMove_RootDeleted_SubInIfKept_DueToIfOp) {
  const auto if_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(if_sub) {
    CHAIN(NODE("if_sub_data", if_sub_data)
              ->EDGE(0, 0)
              ->NODE("if_tensormove", TENSORMOVE)
              ->EDGE(0, 0)
              ->NODE("if_sub_netoutput", NETOUTPUT));
  };
  const auto then_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(then_sub) {
    CHAIN(NODE("then_sub_data", then_sub_data)->NODE("then_relu", RELU)->NODE("then_sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("relu", RELU)
              ->NODE("if", IF, if_sub, then_sub)
              ->NODE("transdata", TRANSDATA)
              ->NODE("tensormove", TENSORMOVE)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto compute_graph = ToComputeGraph(g1);
  const auto then_sub_graph = compute_graph->GetSubgraph("then_sub");
  ASSERT_NE(then_sub_graph, nullptr);
  const auto if_sub_graph = compute_graph->GetSubgraph("if_sub");
  ASSERT_NE(if_sub_graph, nullptr);

  compute_graph->TopologicalSorting();

  NetoutputParentIndexes indexes{{"if_sub_netoutput", {0}}, {"then_sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_EQ(compute_graph->FindNode("tensormove"), nullptr);
}

/**
 * 主图 g1：
 *          data
 *            |
 *            IF
 *            |
 *        tensormove
 *            |
 *        netoutput
 *
 * if_sub：
 *        if_sub_data
 *          |      \
 *          |       \
 *          |        if_tensormove
 *          |           |
 *          |          |
 *          |         |
 *     if_sub_netoutput
 *
 * then 分支子图 then_sub：
 *
 *        then_sub_data
 *              |
 *          then_relu
 *              |
 *      then_sub_netoutput
 *
 * 预期行为：
 * - if 分支子图中的 if_tensormove 不删除，其输入为根图Data,路径上有IF算子
 * - 主图中的 tensormove 不删除，其输入为根图Data,路径上有IF算子
 */
TEST_F(UtestTensorMoveDeletePass, TensorMove_InRootAndSub_ConnectedToIf_Kept) {
  std::map<std::string, std::string> options;
  options[OPTION_OUTPUT_REUSE_INPUT_MEM_INDEXES] = "1,1|0,0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  const auto if_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(if_sub) {
    CHAIN(NODE("if_sub_data", if_sub_data)->EDGE(0, 1)->NODE("if_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("if_sub_data", if_sub_data)->EDGE(0, 1)->NODE("if_sub_netoutput", NETOUTPUT));
    CHAIN(NODE("if_sub_data", if_sub_data)
              ->EDGE(0, 0)
              ->NODE("if_tensormove", TENSORMOVE)
              ->EDGE(0, 0)
              ->NODE("if_sub_netoutput", NETOUTPUT));
  };
  const auto then_sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(then_sub) {
    CHAIN(NODE("then_sub_data", then_sub_data)->NODE("then_relu", RELU)->NODE("then_sub_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("if", IF, if_sub, then_sub)
              ->NODE("tensormove", TENSORMOVE)
              ->NODE("netoutput", NETOUTPUT));
  };

  auto compute_graph = ToComputeGraph(g1);
  const auto then_sub_graph = compute_graph->GetSubgraph("then_sub");
  ASSERT_NE(then_sub_graph, nullptr);
  const auto if_sub_graph = compute_graph->GetSubgraph("if_sub");
  ASSERT_NE(if_sub_graph, nullptr);

  compute_graph->TopologicalSorting();

  NetoutputParentIndexes indexes{{"if_sub_netoutput", {0, 1}}, {"then_sub_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(compute_graph, indexes));

  ge::GEPass pass(compute_graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_NE(compute_graph->FindNode("tensormove"), nullptr);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(if_sub_graph->FindNode("if_tensormove"), nullptr);
  EXPECT_NE(compute_graph->FindNode("tensormove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 *       Data
 *        |
 *     TensorMove
 *        |
 *     NetOutput  (复用输入地址)
 *
 * 说明：
 * - TensorMove 输入为根图Data，直连NetOutput
 * - 设置了输出复用输入内存
 * - 单输入、单输出
 * - 无分支、无子图
 *
 * 预期行为：
 * - 删除TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromData_MemReuse_Deleted2) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_INPUT_REUSE_MEM_INDEXES] = "0";
  ge::GetThreadLocalContext().SetGraphOption(options);
  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  EXPECT_EQ(node1->GetOutDataNodes().size(), 1);

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromData_MemReuse_InvalidIndex) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_INPUT_REUSE_MEM_INDEXES] = "abc";
  ge::GetThreadLocalContext().SetGraphOption(options);
  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromData_MemReuse_OutOfRangeIndex) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_INPUT_REUSE_MEM_INDEXES] = "999999999999999999999999";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}
TEST_F(UtestTensorMoveDeletePass, TensorMoveFromData_MemReuse_PartialInvalidIndex) {
  setenv("DUMP_GRAPH_LEVEL", "2", 1);
  setenv("DUMP_GE_GRAPH", "2", 1);

  std::map<std::string, std::string> options;
  options[OPTION_INPUT_REUSE_MEM_INDEXES] = "12abc";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 1, 1);
  auto node1 = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto node2 = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  AttrUtils::SetInt(data_node->GetOpDesc(), ATTR_NAME_INDEX, 0);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), node1->GetInDataAnchor(0));
  GraphUtils::AddEdge(node1->GetOutDataAnchor(0), node2->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);

  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);
  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);

  ge::GetThreadLocalContext().SetGraphOption({});
}
/**
 *       Variable
 *          |
 *       TensorMove
 *          |
 *         Relu
 *
 * 说明：
 * - TensorMove 源节点为 Variable（特殊数据源节点）
 * - Relu 为只读节点，不会覆写源内存
 *
 * 预期：
 * - 删除 TensorMove，Variable 直连 Relu
 */
TEST_F(UtestTensorMoveDeletePass, DeleteTMWhenSourceIsVariableAndSuccIsReadOnly) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), relu_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_TRUE(variable_node->GetOutDataAnchor(0)->IsLinkedWith(relu_node->GetInDataAnchor(0)));
}

/**
 *       Variable
 *          |
 *       TensorMove
 *          |
 *     OverwriteNode (modify_input)
 *
 * 说明：
 * - TensorMove 源节点为 Variable
 * - 后继节点的输入读写关系为 kScopeWriteable，会覆写源内存
 *
 * 预期：
 * - 保留 TensorMove，不删除
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSourceIsVariableAndSuccOverwritesSource) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto overwrite_node = builder.AddNode("OverwriteNode", ADD, 1, 1);

  SetModifyInput(overwrite_node);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), overwrite_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *         Relu
 *        /    \
 *     TM_A   TM_B
 *       |      |
 *   Relu_op1 Relu_op2
 *
 * 说明：
 * - Relu 单输出被两个 TensorMove 同时引用
 * - Sibling 节点为另一个 TensorMove
 *
 * 预期：
 * - TM_A 被删除，Relu 直连 Relu_op1
 * - TM_B 保留（sibling 为 TM 的特殊分支场景）
 */
TEST_F(UtestTensorMoveDeletePass, DeleteOneTMWhenSiblingIsTM) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto tm_a_node = builder.AddNode("TM_A", TENSORMOVE, 1, 1);
  auto tm_b_node = builder.AddNode("TM_B", TENSORMOVE, 1, 1);
  auto relu_op1_node = builder.AddNode("Relu_op1", RELU, 1, 1);
  auto relu_op2_node = builder.AddNode("Relu_op2", RELU, 1, 1);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tm_a_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm_a_node->GetOutDataAnchor(0), relu_op1_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tm_b_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm_b_node->GetOutDataAnchor(0), relu_op2_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TM_A"), nullptr);
  EXPECT_TRUE(relu_node->GetOutDataAnchor(0)->IsLinkedWith(relu_op1_node->GetInDataAnchor(0)));
}

/**
 *  Variable -> Relu
 *
 * 说明：
 * - Variable 输出标记为 Writeable，Relu 输入为 ReadOnly
 * - 单独调用接口验证无 RW 冲突
 *
 * 预期：
 * - WouldDeleteTensorMoveCauseRWConflict 返回 false
 */
TEST_F(UtestTensorMoveDeletePass, RWConflictInterfaceReportsNoConflict) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), relu_node->GetInDataAnchor(0));

  InitRWConflictCheck(builder.GetGraph());
  EXPECT_FALSE(WouldDeleteTensorMoveCauseRWConflict(variable_node, 0, relu_node, 0));
}

/**
 *  Relu -> TensorMove -> Succ (no overwrite)
 *         \-> Sibling (modify_input, kScopeWriteable)
 *
 * 说明：
 * - Sibling 设置了 _input_mutable=true，RW 类型为 kScopeWriteable
 * - WillNodeOverwriteSourceMemory 检测到 sibling 会覆写源内存
 *
 * 预期：
 * - 不删除 TensorMove（旁路会覆写源内存）
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSiblingHasModifyInput) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);
  auto sibling_node = builder.AddNode("Sibling", ADD, 1, 1);

  SetModifyInput(sibling_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));

  InitRWConflictCheck(builder.GetGraph());

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *  Variable -> TensorMove -> Succ (modify_input, kScopeWriteable)
 *
 * 说明：
 * - Source 是 Variable（特殊源），触发 WouldTMSuccessorsOverwriteSource
 * - TM 后继 Succ 设置了 _input_mutable=true，RW 类型为 kScopeWriteable
 * - WillNodeOverwriteSourceMemory 应检测到后继会覆写源内存
 *
 * 预期：
 * - 不删除 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSuccHasModifyInputAndSourceIsSpecial) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);

  SetModifyInput(succ_node);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  InitRWConflictCheck(builder.GetGraph());

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *  Relu -> TensorMove -> ReadOnlyNode (no overwrite)
 * 只是纯读节点，不覆写源内存
 *
 * 预期：
 * - 删除 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, DeleteTMWhenSuccIsReadOnly) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

// ===== 缺口用例：Variable/Const 源 × 连接方式 × TM路径 × 覆写组合 =====

/**
 * [Gap 1] Variable -> sibling(modify_input), Variable -> TM -> succ(pureRead)
 *
 * 预期：不删除 TM（旁路会覆写源内存）
 */
TEST_F(UtestTensorMoveDeletePass, VariableMultiRefWritableSiblingKept) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);
  auto sibling_node = builder.AddNode("Sibling", ADD, 1, 1);

  SetModifyInput(sibling_node);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 2] Variable -> RefOp(Reshape) -> TM -> succ(pureRead)
 *
 * 预期：删除 TM（间接 RefOp 路径，succ 不改写输入）
 */
TEST_F(UtestTensorMoveDeletePass, VariableViaRefOpToPureReadSuccDeleted) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  // Reshape 是 ref 节点，GE 中 Reshape 默认有 ref 关系
  auto reshape_node = builder.AddNode("Reshape", RESHAPE, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), reshape_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(reshape_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 3] Constant -> TM -> NetOutput
 *
 * 预期：保留 TM（Constant 是特殊源节点，需通过 OptimizeTensorMove 的符号表路径检验）
 */
TEST_F(UtestTensorMoveDeletePass, ConstantDirectToNetOutputKept) {
  auto builder = ut::GraphBuilder("g1");
  auto constant_node = builder.AddNode("Constant", CONSTANT, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 1, 1);

  GraphUtils::AddEdge(constant_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));

  auto graph = builder.GetGraph();
  TensorMoveDeletePass tensor_move_delete_pass;
  ASSERT_EQ(tensor_move_delete_pass.Init(graph), SUCCESS);
  ge::GEPass pass(graph);
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(graph->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 4] Constant -> TM -> overwrite_succ(modify_input)
 *
 * 预期：保留 TM（双重拒绝：Constant 特殊 + 后继覆写）
 */
TEST_F(UtestTensorMoveDeletePass, ConstantDirectToOverwriteSuccKept) {
  auto builder = ut::GraphBuilder("g1");
  auto constant_node = builder.AddNode("Constant", CONSTANT, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto overwrite_node = builder.AddNode("Overwrite", ADD, 1, 1);

  SetModifyInput(overwrite_node);

  GraphUtils::AddEdge(constant_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), overwrite_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 6] Constant -> RefOp(Reshape) -> TM -> succ(pureRead)
 *
 * 预期：删除 TM（Constant 为特殊源但无后继覆写，符号合并后无排布冲突）
 */
TEST_F(UtestTensorMoveDeletePass, ConstantViaRefOpToPureReadSuccDeleted) {
  auto builder = ut::GraphBuilder("g1");
  auto constant_node = builder.AddNode("Constant", CONSTANT, 0, 1);
  auto reshape_node = builder.AddNode("Reshape", RESHAPE, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);

  GraphUtils::AddEdge(constant_node->GetOutDataAnchor(0), reshape_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(reshape_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  auto graph = builder.GetGraph();
  TensorMoveDeletePass tensor_move_delete_pass;
  ASSERT_EQ(tensor_move_delete_pass.Init(graph), SUCCESS);

  ge::GEPass pass(graph);
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(graph->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 7] Data -> sibling(modify_input), Data -> TM -> succ(pureRead)
 *
 * 预期：保留 TM（旁路会覆写源内存）
 */
TEST_F(UtestTensorMoveDeletePass, DataMultiRefWritableSiblingKept) {
  auto builder = ut::GraphBuilder("g1");
  auto data_node = builder.AddNode("Data", DATA, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);
  auto sibling_node = builder.AddNode("Sibling", ADD, 1, 1);
  auto netoutput_node = builder.AddNode("NetOutput", NETOUTPUT, 2, 1);

  SetModifyInput(sibling_node);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(succ_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(sibling_node->GetOutDataAnchor(0), netoutput_node->GetInDataAnchor(1));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 8] Variable -> TM_A -> overwrite_succ, Variable -> TM_B -> pure_succ
 *
 * 预期：TM_A 保留（后继覆写），TM_B 视情况
 */
TEST_F(UtestTensorMoveDeletePass, VariableMultiTMOneOverwriteKept) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tm_a_node = builder.AddNode("TM_A", TENSORMOVE, 1, 1);
  auto tm_b_node = builder.AddNode("TM_B", TENSORMOVE, 1, 1);
  auto overwrite_node = builder.AddNode("Overwrite", ADD, 1, 1);
  auto pure_node = builder.AddNode("PureSucc", RELU, 1, 1);

  SetModifyInput(overwrite_node);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tm_a_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm_a_node->GetOutDataAnchor(0), overwrite_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tm_b_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm_b_node->GetOutDataAnchor(0), pure_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  // TM_A -> overwrite succ: 应保留
  EXPECT_NE(builder.GetGraph()->FindNode("TM_A"), nullptr);
}

/**
 * [Gap 9] ConstructSingleNodeSymbolTable — symbol 不存在于 orig_symbol_to_anchors
 *
 * 说明:
 * - 调用 ConstructSingleNodeSymbolTable，传入不存在的 symbol
 * - 应进入 `sym_iter == orig_symbol_to_anchors.end()` 分支，安全 return
 *
 * 预期：不崩溃，输出表为空
 */
TEST_F(UtestTensorMoveDeletePass, ConstructSingleNodeSymbolTableNotFound) {
  AnchorToSymbol orig_anchor_to_symbol;
  SymbolToAnchors orig_symbol_to_anchors;
  // 预先放入一个无关 symbol，确保遍历主路径不受影响
  NodeIndexIO dummy_io(nullptr, 0, kIn);
  orig_anchor_to_symbol[dummy_io.ToString()] = "exist_sym";
  orig_symbol_to_anchors["exist_sym"].push_back(dummy_io);

  AnchorToSymbol out_anchor_to_symbol;
  SymbolToAnchors out_symbol_to_anchors;

  MemLayoutConflictUtil::ConstructSingleNodeSymbolTable("not_found_input", "not_found_output", orig_anchor_to_symbol,
                                                        orig_symbol_to_anchors, out_anchor_to_symbol,
                                                        out_symbol_to_anchors);

  // 预期输出表保持为空（所有 copy_symbol_anchors 调用都是 no-op）
  EXPECT_TRUE(out_anchor_to_symbol.empty());
  EXPECT_TRUE(out_symbol_to_anchors.empty());
}

/**
 * [Gap 10] CheckMultiConsumerDeleteRule — sibling 是 NetOutput 时返回 false
 *
 * 说明：
 * - 旁路消费者是 NetOutput 类型时，不支持多消费者删除规则
 * - 这覆盖了移除 IsTensorMove 检查后的 NetOutput 判断分支
 *
 * 预期：保留 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSiblingIsNetOutput) {
  auto builder = ut::GraphBuilder("g1");
  auto src_node = builder.AddNode("Src", RELU, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);
  auto sibling_node = builder.AddNode("SiblingNetOutput", NETOUTPUT, 1, 1);

  GraphUtils::AddEdge(src_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(src_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 11] 走 CheckRWConflictOnDelete 的 RW 冲突路径
 *
 * 说明：
 * - Variable (writable) → TensorMove → Succ(modify_input)
 * - InitRWConflictCheck + Init 建立符号表
 * - 触发 CheckRWConflictOnDelete 中 WouldDeleteTensorMoveCauseRWConflict 返回 true
 *
 * 预期：保留 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenRWConflictDetected) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);

  SetModifyInput(succ_node);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  InitRWConflictCheck(builder.GetGraph());

  auto graph = builder.GetGraph();
  TensorMoveDeletePass tensor_move_delete_pass;
  ASSERT_EQ(tensor_move_delete_pass.Init(graph), SUCCESS);

  ge::GEPass pass(graph);
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(graph->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 12] CheckMemLayoutConflictOnDelete — TM 两端符号相同，允许删除
 *
 * 说明：
 * - 通过 Ref 链确保 TM 输入/输出锚点被映射到同一个 symbol
 * - CheckMemLayoutConflictOnDelete 中 in_iter->second == out_iter->second → return true
 *
 * 预期：删除 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, DeleteTMWhenAnchorsShareSameSymbol) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto reshape_node = builder.AddNode("Reshape", RESHAPE, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", RELU, 1, 1);

  // Reshape 是 ref 节点，这条链在 GetRefMapping 时会连成同一个 symbol
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), reshape_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(reshape_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));

  auto graph = builder.GetGraph();
  TensorMoveDeletePass tensor_move_delete_pass;
  ASSERT_EQ(tensor_move_delete_pass.Init(graph), SUCCESS);

  ge::GEPass pass(graph);
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_EQ(graph->FindNode("TensorMove"), nullptr);
}

/**
 * [Gap 13] CheckMemLayoutConflictOnDelete — MemLayout merge causes memory conflict
 *
 * 说明：
 * - var1 → TM1 → PhonyConcat, var2 → TM2 → PhonyConcat
 * - PhonyConcat: nopadding_continuous_input + output_ref_input
 * - Init 建立符号表，CheckMemLayoutConflictOnDelete 调用
 *   ConstructSingleNodeSymbolTable → IsGraphExistMemConflictSymbol
 * - 两根不同 Variable 链被合并到同一个连续内存 PhonyConcat → has_conflict = true
 *
 * 预期：保留 TM1（内存布局冲突阻止删除）
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenMemLayoutConflictAfterSymbolMerge) {
  auto builder = ut::GraphBuilder("g1");
  auto var1 = builder.AddNode("Var1", VARIABLE, 0, 1);
  auto var2 = builder.AddNode("Var2", VARIABLE, 0, 1);
  auto tm1 = builder.AddNode("TM1", TENSORMOVE, 1, 1);
  auto tm2 = builder.AddNode("TM2", TENSORMOVE, 1, 1);
  auto phony_concat = builder.AddNode("PhonyConcat", PHONYCONCAT, 2, 1);

  // PhonyConcat: nopadding_continuous_input + output refs input
  AttrUtils::SetBool(phony_concat->GetOpDesc(), ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  AttrUtils::SetBool(phony_concat->GetOpDesc(), ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  AttrUtils::SetInt(phony_concat->GetOpDesc(), ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);
  auto pc_out_desc = phony_concat->GetOpDescBarePtr()->MutableOutputDesc(0);
  TensorUtils::SetReuseInput(*pc_out_desc, true);
  TensorUtils::SetReuseInputIndex(*pc_out_desc, 0U);

  // Variables: mark with ref_var_src to establish ref chain origins
  AttrUtils::SetStr(var1->GetOpDescBarePtr()->MutableOutputDesc(0), REF_VAR_SRC_VAR_NAME, "Var1");
  AttrUtils::SetStr(var2->GetOpDescBarePtr()->MutableOutputDesc(0), REF_VAR_SRC_VAR_NAME, "Var2");

  // var1 → TM1 → PhonyConcat(in 0)
  GraphUtils::AddEdge(var1->GetOutDataAnchor(0), tm1->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm1->GetOutDataAnchor(0), phony_concat->GetInDataAnchor(0));
  // var2 → TM2 → PhonyConcat(in 1)
  GraphUtils::AddEdge(var2->GetOutDataAnchor(0), tm2->GetInDataAnchor(0));
  GraphUtils::AddEdge(tm2->GetOutDataAnchor(0), phony_concat->GetInDataAnchor(1));

  auto graph = builder.GetGraph();
  TensorMoveDeletePass tensor_move_delete_pass;
  ASSERT_EQ(tensor_move_delete_pass.Init(graph), SUCCESS);

  ge::GEPass pass(graph);
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(graph->FindNode("TM1"), nullptr);
}

/**
 *  Variable -> TensorMove -> RefSucc
 *
 * 说明：
 * - Source 是 Variable（特殊源），触发 IsSuccessorSafeAfterTensorMove
 * - TensorMove 后继 RefSucc 的输出复用来自 TensorMove 的输入
 *
 * 预期：保留 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSpecialSourceSuccessorIsRefOp) {
  auto builder = ut::GraphBuilder("g1");
  auto variable_node = builder.AddNode("Variable", VARIABLE, 0, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto ref_succ_node = builder.AddNode("RefSucc", ADD, 1, 1);

  SetRefOutput(ref_succ_node, 0, 0);

  GraphUtils::AddEdge(variable_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), ref_succ_node->GetInDataAnchor(0));

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
}

/**
 *  Variable -> TensorMove -> PartitionedCall
 *
 * 说明：
 * - Source 是 Variable（特殊源），触发 IsSuccessorSafeAfterTensorMove
 * - TensorMove 后继 PartitionedCall 持有子图，是 wrapper node
 *
 * 预期：保留 TensorMove
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenSpecialSourceSuccessorIsWrapperNode) {
  DEF_GRAPH(sub) {
    CHAIN(NODE("sub_data", OP_CFG(DATA).ParentNodeIndex(0))->NODE("sub_netoutput", NETOUTPUT));
  };

  DEF_GRAPH(g1) {
    CHAIN(NODE("Variable", VARIABLE)->NODE("TensorMove", TENSORMOVE)->NODE("PartitionedCall", PARTITIONEDCALL, sub));
  };

  auto graph = ToComputeGraph(g1);
  ASSERT_NE(graph, nullptr);

  ge::GEPass pass(graph);
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(graph->FindNode("TensorMove"), nullptr);
}

/**
 *  Relu -> TensorMove -> Succ(modify_input, kScopeWriteable)
 *    \-> Sibling(pure read)
 *
 *  说明：
 *  - Relu 输出被 TensorMove 和纯读 Sibling 同时消费，触发 basic multi-ref 规则登记 pending 控制边
 *  - Succ 设置 _input_mutable=true，RW 类型为 kScopeWriteable
 *  - CheckRWConflictOnDelete 基于当前图判定 Relu(out:0) ReadOnly -> Succ(in:0) ScopeWriteable 冲突
 *
 *  预期：
 *  - 保留 TensorMove
 *  - pending 控制边不会落地
 */
TEST_F(UtestTensorMoveDeletePass, KeepTMWhenPendingOrderDoesNotBypassRWConflict) {
  auto builder = ut::GraphBuilder("g1");
  auto relu_node = builder.AddNode("Relu", RELU, 1, 1);
  auto tensor_move_node = builder.AddNode("TensorMove", TENSORMOVE, 1, 1);
  auto succ_node = builder.AddNode("Succ", ADD, 1, 1);
  auto sibling_node = builder.AddNode("Sibling", ADD, 1, 1);

  SetModifyInput(succ_node);

  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), tensor_move_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(tensor_move_node->GetOutDataAnchor(0), succ_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(relu_node->GetOutDataAnchor(0), sibling_node->GetInDataAnchor(0));

  InitRWConflictCheck(builder.GetGraph());

  ge::GEPass pass(builder.GetGraph());
  TensorMoveDeletePass tensor_move_delete_pass;
  ge::NamesToPass names_to_pass;
  names_to_pass.emplace_back("TensorMoveDeletePass", &tensor_move_delete_pass);
  EXPECT_EQ(pass.Run(names_to_pass), SUCCESS);

  EXPECT_NE(builder.GetGraph()->FindNode("TensorMove"), nullptr);
  EXPECT_FALSE(sibling_node->GetOutControlAnchor()->IsLinkedWith(succ_node->GetInControlAnchor()));
}
