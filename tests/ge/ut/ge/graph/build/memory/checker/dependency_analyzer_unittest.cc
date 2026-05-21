/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "graph/build/memory/checker/dependency_analyzer.h"
#include "common/share_graph.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/utils/graph_utils_ex.h"
#include "stub/gert_runtime_stub.h"
#include "graph/ge_local_context.h"
#include "graph/ge_context.h"
#include "common/ge_common/ge_types.h"
#include "framework/common/types.h"
#include "../../test_memory_shared_graph.h"
#include "register/optimization_option_registry.h"

namespace ge {
namespace {
struct NodeNameIndex {
  std::string name;
  uint32_t index;
};
void SetAllNodesInvalidStream(const ComputeGraphPtr &graph) {
  for (const auto &node : graph->GetAllNodes()) {
    node->GetOpDescBarePtr()->SetStreamId(-1);
  }
}
void SetAllNodesDiffStream(const ComputeGraphPtr &graph) {
  int64_t stream_id = 0;
  for (const auto &node : graph->GetAllNodes()) {
    node->GetOpDescBarePtr()->SetStreamId(stream_id++);
  }
}
bool CheckAllReusableNodes(const std::unordered_map<std::string, NodePtr> &name_to_node,
                           const std::unordered_set<OutDataAnchorPtr> &out_anchors,
                           const std::vector<NodeNameIndex> &node_out_indexes) {
  bool success_flag = true;
  if (out_anchors.size() != node_out_indexes.size()) {
    return false;
  }
  for (const auto &node_out_index : node_out_indexes) {
    const auto iter = name_to_node.find(node_out_index.name);
    if ((iter == name_to_node.end()) || (iter->second == nullptr)) {
      success_flag = false;
      std::cout << "cannot find " << node_out_index.name << std::endl;
      continue;
    }
    const auto node = iter->second;
    auto anchor = node->GetOutDataAnchor(node_out_index.index);
    if (anchor == nullptr) {
      std::cout << "anchor is null, node: " << node_out_index.name << ", index: " << node_out_index.index << std::endl;
      success_flag = false;
    }
    EXPECT_NE(anchor, nullptr);
    const auto anchor_iter = out_anchors.find(anchor);
    if (anchor_iter == out_anchors.cend()) {
      std::cout << "anchor not found, node: " << node_out_index.name << ", index: " << node_out_index.index
                << std::endl;
      success_flag = false;
    }
    EXPECT_NE(anchor_iter, out_anchors.cend());
  }
  return success_flag;
}

std::unordered_set<OutDataAnchorPtr> GetAllReuableOutAnchors(ComputeGraphPtr &graph, const DependencyAnalyzer &nmda,
                                                             const NodePtr &cur_node) {
  std::unordered_set<OutDataAnchorPtr> ret;
  for (const auto node : graph->GetAllNodesPtr()) {
    for (const auto &out_anchor : node->GetAllOutDataAnchors()) {
      if (nmda.CanAReuseB(cur_node.get(), 0, node, out_anchor->GetIdx())) {
        ret.insert(out_anchor);
      }
    }
  }
  return ret;
}

using ReusalbeMap = std::map<std::string, std::vector<NodeNameIndex>>;
bool CheckAllReusableNodes(ComputeGraphPtr &graph, const ReusalbeMap &reusable_map) {
  bool success_flag = true;
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();

  std::unordered_map<std::string, NodePtr> name_to_node;
  for (const auto &node : graph->GetAllNodes()) {
    name_to_node[node->GetName()] = node;
  }

  for (const auto &name_to_anchor : reusable_map) {
    const auto cur_node = name_to_node[name_to_anchor.first];
    EXPECT_NE(cur_node, nullptr);
    if (cur_node == nullptr) {
      std::cout << "node: " << name_to_anchor.first << " is not found" << std::endl;
      success_flag = false;
      continue;
    }
    const auto reusable_nodes = GetAllReuableOutAnchors(graph, nmda, cur_node);
    EXPECT_EQ(reusable_nodes.size(), name_to_anchor.second.size());
    if (reusable_nodes.size() != name_to_anchor.second.size()) {
      std::stringstream ss;
      ss << "expect reuse node [";
      for (const auto &name_index : name_to_anchor.second) {
        ss << "[" << name_index.name << ", " << name_index.index << "], ";
      }
      ss << "]\n actual reuse node [";
      for (const auto &anchor : reusable_nodes) {
        ss << "[" << anchor->GetOwnerNodeBarePtr()->GetName() << ", " << anchor->GetIdx() << "], ";
      }
      ss << "].";
      std::cout << "node: " << name_to_anchor.first << " reusable nodes check failed. \n"
                << "expect size: " << name_to_anchor.second.size() << ", actual size: " << reusable_nodes.size()
                << "\n " << ss.str() << std::endl;
      success_flag = false;
      continue;
    }
    EXPECT_TRUE(CheckAllReusableNodes(name_to_node, reusable_nodes, name_to_anchor.second));
  }
  return success_flag;
}

void CheckNoReusalbeNodes(ComputeGraphPtr &graph, const std::vector<std::string> &names) {
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  for (const auto &name : names) {
    const auto cur_node = graph->FindNode(name);
    ASSERT_NE(cur_node, nullptr);
    const auto reusable_nodes = GetAllReuableOutAnchors(graph, nmda, cur_node);
    if (!reusable_nodes.empty()) {
      std::cerr << "node: " << name << ", has reusabel nodes: " << (*reusable_nodes.begin())->GetOwnerNode()->GetName()
                << std::endl;
    }
    EXPECT_TRUE(reusable_nodes.empty());
  }
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
      GE_DUMP(root_graph, "AddParentIndexForNetoutput_failed");
      return false;
    }
    auto op_desc = iter->second->GetOpDesc();
    size_t input_index = 0U;
    if (name_indexes_pair.second.size() != op_desc->GetInputsSize()) {
      std::cout << "========================================" << std::endl;
      std::cout << name_indexes_pair.first << " real inputs size: " << op_desc->GetInputsSize()
                << ", but name_indexes_pair.second.size(): " << name_indexes_pair.second.size() << std::endl;
      std::cout << "========================================" << std::endl;
      GE_DUMP(root_graph, "AddParentIndexForNetoutput_failed");
      return false;
    }
    for (auto parent_index : name_indexes_pair.second) {
      auto tensor_desc = op_desc->MutableInputDesc(input_index++);
      AttrUtils::SetInt(tensor_desc, ATTR_NAME_PARENT_NODE_INDEX, parent_index);
    }
  }
  return true;
}
}  // namespace
class UtestDependencyAnalyzer : public testing::Test {
 protected:
  void SetUp() {
    global_options_ = ge::GetThreadLocalContext().GetAllGlobalOptions();
    graph_options_ = ge::GetThreadLocalContext().GetAllGraphOptions();
    session_options_ = ge::GetThreadLocalContext().GetAllSessionOptions();

    std::map<std::string, std::string> tmp_global_option;
    tmp_global_option.insert(std::make_pair(ge::OPTION_TOPOSORTING_MODE, "0"));
    ge::GetThreadLocalContext().SetGlobalOption(tmp_global_option);
    GetThreadLocalContext().GetOo().Initialize(GetThreadLocalContext().GetAllOptions(),
                                               OptionRegistry::GetInstance().GetRegisteredOptTable());
  }
  void TearDown() {
    ge::GetThreadLocalContext().SetGlobalOption(global_options_);
    ge::GetThreadLocalContext().SetGraphOption(graph_options_);
    ge::GetThreadLocalContext().SetSessionOption(session_options_);
    GetThreadLocalContext().GetOo().Initialize(GetThreadLocalContext().GetAllOptions(),
                                               OptionRegistry::GetInstance().GetRegisteredOptTable());
  }
  std::map<std::string, std::string> global_options_;
  std::map<std::string, std::string> graph_options_;
  std::map<std::string, std::string> session_options_;
};

// ┌────────┐  (1,0)   ┌───┐  (0,0)   ┌───┐  (0,1)   ┌───────────┐
// │   A    │ ───────> │ D │ ───────> │ E │ ───────> │ NetOutput │
// └────────┘          └───┘          └───┘          └───────────┘
//   │                                                 ∧
//   │ (0,0)                                           │
//   ∨                                                 │
// ┌────────┐  (0,0)   ┌───┐  (0,0)                    │
// │   B    │ ───────> │ C │ ──────────────────────────┘
// └────────┘          └───┘
TEST_F(UtestDependencyAnalyzer, SinleOutSingleReference) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B")->NODE("C", "C")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("A", "A")->NODE("D", "D")->NODE("E", "E")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  ReusalbeMap reusable_map;
  reusable_map["C"] = {{"A", 0}};
  reusable_map["E"] = {{"A", 1}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "D"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

//                     (0,0)
//                 ┌──────────────┐
//                 │              │
//                 │              │        (0,1)
//            ┌────┼──────────────┼─────────────────────────────────────────────────┐
//            │    ∨              │                                                 ∨
//┌────────┐  │  ┌───┐          ┌────────┐  (1,0)   ┌───┐  (0,0)   ┌───┐  (0,2)   ┌───────────┐
//│   E    │ ─┘  │ C │ ─┐       │   A    │ ───────> │ F │ ───────> │ G │ ───────> │ NetOutput │
//└────────┘     └───┘  │       └────────┘          └───┘          └───┘          └───────────┘
//  ∧              │    │         │                                                 ∧
//  │ (0,0)        │    │         │ (0,0)                                           │
//  │              │    │         ∨                                                 │
//  │              │    │       ┌────────┐  (0,0)   ┌───┐  (0,0)                    │
//  └──────────────┼────┘       │   B    │ ───────> │ D │ ──────────────────────────┘
//                 │            └────────┘          └───┘
//                 │   (1,1)                          ∧
//                 └──────────────────────────────────┘
//
// A的第0个输出是单数出多引用，给到BC，BC连接D，所以D可以复用A的第0个输出；
// 由于B无法到达E，所以E无法复用A的内存。
// A的第1个输出可以被G复用。
TEST_F(UtestDependencyAnalyzer, SinleOutMultiReference) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B")->NODE("D", "D")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("C", "C")->NODE("E", "E")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("C", "C")->NODE("D", "D"));
    CHAIN(NODE("A", "A")->EDGE(1, 0)->NODE("F", "F")->NODE("G", "G")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  ReusalbeMap reusable_map;
  reusable_map["D"] = {{"A", 0}};
  reusable_map["G"] = {{"A", 1}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "C", "F"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

//                     ┌────────┐
//                     │   E    │
//                     └────────┘
//                       ∧
//                       │ (1,0)
//                       │
// ┌────────┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────┐
// │   A    │ ───────> │   B    │ ───────> │ C │ ───────> │ F │ ───────> │ NetOutput │
// └────────┘          └────────┘          └───┘          └───┘          └───────────┘
//   │                            (1,0)      │                             ∧
//   │ (0,0)             ┌───────────────────┘                             │
//   ∨                   ∨                                                 │
// ┌────────┐  (0,1)   ┌────────┐  (0,1)                                   │
// │   D    │ ───────> │   G    │ ─────────────────────────────────────────┘
// └────────┘          └────────┘
// B的第0个输出引用输入。
// A的一个输出给到B和D，虽然B和D都到达E，但是E无法复用A的内存。因为A的输出内存也会给到C，C和E并行。
// ABCD都可以到达的点，才能复用A。G可以复用A
TEST_F(UtestDependencyAnalyzer, RefNodeCheckBeReused) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B")->NODE("C", "C")->NODE("F", "F")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("B", "B")->NODE("E", "E"));
    CHAIN(NODE("C", "C")->NODE("G", "G"));
    CHAIN(NODE("D", "D")->NODE("E", "E"));
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("D", "D")->NODE("G", "G")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);
  // b is RefNode
  auto b = graph->FindNode("B");
  ASSERT_NE(b, nullptr);
  auto b_op_desc = b->GetOpDesc();
  ASSERT_NE(b_op_desc, nullptr);
  (void)ge::AttrUtils::SetBool(b_op_desc, ATTR_NAME_REFERENCE, true);
  b_op_desc->UpdateOutputName({std::make_pair("x", 0), std::make_pair("y", 1)});
  b_op_desc->UpdateInputName({std::make_pair("x", 0)});

  ReusalbeMap reusable_map;
  reusable_map["G"] = {{"A", 0}, {"B", 0}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "C", "D", "E", "F"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

// ┌───┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌───────────┐
// │ A │ ───────> │   B    │ ───────> │ C │ ───────> │ NetOutput │
// └───┘          └────────┘          └───┘          └───────────┘
//                  ∧
//                  │ (0,1)
//                  │
// ┌───┐  (0,0)   ┌────────┐
// │ D │ ───────> │   E    │
// └───┘          └────────┘
// B的第0个输出引用输入。B的输出不能复用D的输出内存
TEST_F(UtestDependencyAnalyzer, RefNodeCheckReuseOthers_NotReuse) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B")->NODE("C", "C")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("D", "D")->NODE("E", "E")->NODE("B", "B"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);
  // b is RefNode
  auto b = graph->FindNode("B");
  ASSERT_NE(b, nullptr);
  auto b_op_desc = b->GetOpDesc();
  ASSERT_NE(b_op_desc, nullptr);
  (void)ge::AttrUtils::SetBool(b_op_desc, ATTR_NAME_REFERENCE, true);
  b_op_desc->UpdateOutputName({std::make_pair("x", 0)});
  b_op_desc->UpdateInputName({std::make_pair("x", 0), std::make_pair("y", 1)});

  ReusalbeMap reusable_map;
  reusable_map["C"] = {{"D", 0}, {"E", 0}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "D", "E"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

// ┌────────┐  (1,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────┐
// │   D    │ ───────> │ F │ ───────> │ A │ ───────> │ B │ ───────> │ NetOutput │
// └────────┘          └───┘          └───┘          └───┘          └───────────┘
//   │                                                 ∧
//   │ (0,0)                                           │
//   ∨                                                 │
// ┌────────┐  (0,1)                                   │
// │   E    │ ─────────────────────────────────────────┘
// └────────┘
// B的第0个输出引用输入。AB可以使用D的1th输出内存
TEST_F(UtestDependencyAnalyzer, RefNodeCheckReuseOthers_CanReuse) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("D", "D")->NODE("E", "E")->NODE("B", "B"));
    CHAIN(NODE("D", "D")->NODE("F", "F")->NODE("A", "A"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);
  // b is RefNode
  auto b = graph->FindNode("B");
  ASSERT_NE(b, nullptr);
  auto b_op_desc = b->GetOpDesc();
  ASSERT_NE(b_op_desc, nullptr);
  (void)ge::AttrUtils::SetBool(b_op_desc, ATTR_NAME_REFERENCE, true);
  b_op_desc->UpdateOutputName({std::make_pair("x", 0)});
  b_op_desc->UpdateInputName({std::make_pair("x", 0), std::make_pair("y", 1)});

  ReusalbeMap reusable_map;
  reusable_map["B"] = {{"D", 1}};
  reusable_map["A"] = {{"D", 1}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"D", "E", "F"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

void SetNoPaddingContinousInput(ComputeGraphPtr &graph, const std::string &name) {
  auto node = graph->FindNode(name);
  ASSERT_NE(node, nullptr);
  auto op_desc = node->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)AttrUtils::SetBool(op_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
}

void SetNoPaddingContinousOutput(ComputeGraphPtr &graph, const std::string &name) {
  auto node = graph->FindNode(name);
  ASSERT_NE(node, nullptr);
  auto op_desc = node->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NOPADDING_CONTINUOUS_OUTPUT, true);
  (void)AttrUtils::SetBool(op_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
}

void SetStreamForNodes(ComputeGraphPtr &graph, const std::vector<std::string> &names, const int64_t stream_id) {
  for (const auto &name : names) {
    auto node = graph->FindNode(name);
    ASSERT_NE(node, nullptr);
    auto op_desc = node->GetOpDesc();
    ASSERT_NE(op_desc, nullptr);
    op_desc->SetStreamId(stream_id);
  }
}

// ───┐  (0,0)   ┌─────────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐   (0,0)   ┌───┐ (0,0)   ┌───────────┐
//  A │ ───────> │ PhonyConcat │ ───────> │ D │ ───────> │ F │  ───────> │ G │───────> │ NetOutput │
// ───┘          └─────────────┘          └───┘          └───┘           └───┘         └───────────┘
//                 ∧
//                 │ (0,1)
//                 │
//               ┌─────────────┐
//               │      B      │
//               └─────────────┘
// PhonyConcat需要no
// pading连续内存，并且输出引用输入。所以A/B/Phonyconcat成为了一个整体，它们的输出要么都被复用，要么都不能被复用。
// 所以D节点不能复用B的输出内存
TEST_F(UtestDependencyAnalyzer, NoPaddingContinuousInputsCheckBeReused) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")
              ->EDGE(0, 0)
              ->NODE("PhonyConcat", "PhonyConcat")
              ->NODE("D", "D")
              ->NODE("F", "F")
              ->NODE("G", "G")
              ->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("B", "B")->NODE("PhonyConcat", "PhonyConcat"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  // no pading continuous inputs
  SetNoPaddingContinousInput(graph, "PhonyConcat");

  ReusalbeMap reusable_map;
  reusable_map["F"] = {{"A", 0}, {"B", 0}, {"PhonyConcat", 0}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "PhonyConcat", "D"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

//┌───┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌─────────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────┐
//│ A │ ───────> │ B │ ───────> │ E │ ───────> │ PhonyConcat │ ───────> │ G │ ───────> │ H │ ───────> │ NetOutput │
//└───┘          └───┘          └───┘          └─────────────┘          └───┘          └───┘          └───────────┘
//                 │   (1,0)                     ∧
//                 └──────────────┐              │
//                                ∨              │
//┌───┐  (0,0)   ┌───┐  (0,1)   ┌───┐  (0,1)     │
//│ C │ ───────> │ D │ ───────> │ F │ ───────────┘
//└───┘          └───┘          └───┘
// PhonyConcat需要no
// pading连续内存，并且输出引用输入。所以E/F/Phonyconcat成为了一个整体，它们的输出要么都被复用，要么都不能被复用。
// 由于C不能到达E，所以E/F/PhonyConcat都不能复用C的内存
TEST_F(UtestDependencyAnalyzer, NoPaddingContinuousInputsCheckReusOthers) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")
              ->NODE("B", "B")
              ->NODE("E", "E")
              ->NODE("PhonyConcat", "PhonyConcat")
              ->NODE("G", "G")
              ->NODE("H", "H")
              ->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("B", "B")->NODE("F", "F")->NODE("PhonyConcat", "PhonyConcat"));
    CHAIN(NODE("C", "C")->NODE("D", "D")->NODE("F", "F"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  // no pading continuous inputs
  SetNoPaddingContinousInput(graph, "PhonyConcat");

  ReusalbeMap reusable_map;
  reusable_map["E"] = {{"A", 0}};
  reusable_map["F"] = {{"A", 0}};
  reusable_map["PhonyConcat"] = {{"A", 0}};
  reusable_map["G"] = {{"A", 0}, {"B", 0}, {"B", 1}, {"C", 0}, {"D", 0}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "C", "D"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

// PhonyConcat需要nopading连续内存，并且输出引用输入。所以E_0out/D/Phonyconcat成为了一个整体，它们的输出要么都被复用，要么都不能被复用。
// 由于A无法到达D，虽然A能到达C，但是C_0_out不能复用A，但是C_1_out可以复用A
TEST_F(UtestDependencyAnalyzer, NoPaddingContinuousInputsHasMultiOutCheckReusOthers) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")
              ->NODE("B", "B")
              ->NODE("C", "C")
              ->NODE("PhonyConcat", "PhonyConcat")
              ->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("D", "D")->NODE("PhonyConcat", "PhonyConcat"));
    CHAIN(NODE("C", "C")->NODE("E", "E")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  // no pading continuous inputs
  SetNoPaddingContinousInput(graph, "PhonyConcat");

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();

  std::unordered_map<std::string, NodePtr> name_to_node;
  for (const auto &node : graph->GetAllNodes()) {
    name_to_node[node->GetName()] = node;
  }

  ASSERT_TRUE(nmda.CanAReuseB(name_to_node["C"].get(), 1U, name_to_node["A"].get(), 0U));
  ASSERT_FALSE(nmda.CanAReuseB(name_to_node["C"].get(), 0U, name_to_node["A"].get(), 0U));
  nmda.WhyACannotReuseB(name_to_node["C"].get(), 0U, name_to_node["A"].get(), 0U);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "D", "PhonyConcat"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

// ┌───┐  (0,0)   ┌────────┐  (1,0)   ┌───┐  (0,1)   ┌───┐  (0,1)   ┌───────────┐
// │ A │ ───────> │   B    │ ───────> │ D │ ───────> │ F │ ───────> │ NetOutput │
// └───┘          └────────┘          └───┘          └───┘          └───────────┘
//                  │                                  ∧              ∧
//                  │ (0,0)                            │              │
//                  ∨                                  │              │
//                ┌────────┐  (1,0)                    │              │
//                │   C    │ ──────────────────────────┘              │
//                └────────┘                                          │
//                  │                                                 │
//                  │ (0,0)                                           │
//                  ∨                                                 │
//                ┌────────┐  (0,0)                                   │
//                │   E    │ ─────────────────────────────────────────┘
//                └────────┘
//
// B需要连续输出内存，并引用A的输入。E不能复用B的输出内存，因为F无法到达E，F可以复用AB输出内存
TEST_F(UtestDependencyAnalyzer, NoPaddingContinuousOutputCheckBeReused) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B")->NODE("C", "C")->NODE("E", "E")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("C", "C")->NODE("F", "F"));
    CHAIN(NODE("B", "B")->NODE("D", "D")->NODE("F", "F")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  // no pading continuous inputs
  SetNoPaddingContinousOutput(graph, "B");

  ReusalbeMap reusable_map;
  reusable_map["F"] = {{"A", 0}, {"B", 0}, {"B", 1}};
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "C", "D", "E"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

// ┌──────────────┐  (0,0)   ┌──────────────┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌──────────────┐  (0,0)
// ┌───────────┐ │      A       │ ───────> │ PhonyConcat0 │ ───────> │   C    │ ───────> │ D │ ───────> │ PhonyConcat2 │
// ───────> │ NetOutput │ └──────────────┘          └──────────────┘          └────────┘          └───┘ └──────────────┘
// └───────────┘
//                             ∧                         │                                  ∧
//                             │ (0,1)                   │ (1,0)                            │
//                             │                         │                                  │
// ┌──────────────┐          ┌──────────────┐            │                                  │
// │      G       │          │      B       │            │                                  │
// └──────────────┘          └──────────────┘            │                                  │
//   │                                                   │                                  │
//   │ (0,1)                                             │                                  │
//   ∨                                                   ∨                                  │
// ┌──────────────┐  (0,0)   ┌──────────────┐  (0,1)   ┌────────┐  (0,1)                    │
// │ PhonyConcat1 │ ───────> │      H       │ ───────> │   E    │ ──────────────────────────┘
// └──────────────┘          └──────────────┘          └────────┘
//   ∧
//   │ (0,0)
//   │
// ┌──────────────┐
// │      F       │
// └──────────────┘
// group0:A,B,PhonyConcat0;
// group1:F,G,PhonyConcat1;
// group2:D,E,PhonyConcat2：
// group2中的都可以复用group0的，group2不能复用group1的
TEST_F(UtestDependencyAnalyzer, MultiPhonyConcatCheckBeReusedAndReuseOthers) {
  DEF_GRAPH(g1) {
    // group 0
    CHAIN(NODE("A", "A")->NODE("PhonyConcat0", "PhonyConcat0")->NODE("C", "C"));
    CHAIN(NODE("B", "B")->NODE("PhonyConcat0", "PhonyConcat0"));

    // group1
    CHAIN(NODE("F", "F")->NODE("PhonyConcat1", "PhonyConcat1")->NODE("H", "H"));
    CHAIN(NODE("G", "G")->NODE("PhonyConcat1", "PhonyConcat1"));

    // group2
    CHAIN(NODE("D", "D")->NODE("PhonyConcat2", "PhonyConcat2")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("E", "E")->NODE("PhonyConcat2", "PhonyConcat2"));

    CHAIN(NODE("C", "C")->NODE("D", "D"));
    CHAIN(NODE("C", "C")->NODE("E", "E"));
    CHAIN(NODE("H", "H")->NODE("E", "E"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesDiffStream(graph);

  // no pading continuous inputs
  SetNoPaddingContinousInput(graph, "PhonyConcat0");
  SetNoPaddingContinousInput(graph, "PhonyConcat1");
  SetNoPaddingContinousInput(graph, "PhonyConcat2");

  ReusalbeMap reusable_map;
  std::vector<NodeNameIndex> nodes_group0 = {{"A", 0}, {"B", 0}, {"PhonyConcat0", 0}};
  reusable_map["D"] = nodes_group0;
  reusable_map["D"] = nodes_group0;
  reusable_map["E"] = nodes_group0;
  reusable_map["PhonyConcat2"] = nodes_group0;
  CheckAllReusableNodes(graph, reusable_map);

  std::vector<std::string> no_reusalbe_nodes = {"A", "B", "PhonyConcat0", "C", "F", "G", "PhonyConcat1", "H"};
  CheckNoReusalbeNodes(graph, no_reusalbe_nodes);
}

TEST_F(UtestDependencyAnalyzer, CanAReuseBOutMem_Failed_NotInit) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  SetAllNodesInvalidStream(graph);

  const auto a = graph->FindNode("A");
  const auto b = graph->FindNode("B");

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);
  EXPECT_FALSE(nmda.CanAReuseB(a.get(), 0, b.get(), 0));

  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindErrorLogEndsWith("is not inited."), 0);
}

TEST_F(UtestDependencyAnalyzer, GetAllReusableNodesByOutputMemory_Failed_NotInit) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  SetAllNodesInvalidStream(graph);

  const auto a = graph->FindNode("A");

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);
  EXPECT_TRUE(GetAllReuableOutAnchors(graph, nmda, a).empty());

  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindErrorLogEndsWith("is not inited."), 0);
}

TEST_F(UtestDependencyAnalyzer, CanAReuseBOutMem_Failed_ParamIsNullptr) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesInvalidStream(graph);

  const auto a = graph->FindNode("A");

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  EXPECT_FALSE(nmda.CanAReuseB(nullptr, 0, a.get(), 0));

  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindErrorLogEndsWith("invalid param."), 0);
}

TEST_F(UtestDependencyAnalyzer, Init_Failed_WhenIdDuplicated) {
  DEF_GRAPH(g1) {
                  CHAIN(NODE("A", "A")->NODE("B", "B"));
                };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();

  const auto b = graph->FindNode("B");
  b->GetOpDescBarePtr()->SetId(0);

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);
  dlog_setlevel(GE_MODULE_NAME, 2, 0); // 需要warning校验日志
  ASSERT_NE(nmda.Init(), SUCCESS);

  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindWarnLogEndsWith("InitTables:topo id duplicated, id: 0, origin node: A, current node: B"), 0);
  dlog_setlevel(GE_MODULE_NAME, 3, 0);
}

TEST_F(UtestDependencyAnalyzer, CanAReuseB_ReturnTrue_WhenSymbolNotFound) {
  DEF_GRAPH(g1) {
                  CHAIN(NODE("A", "A")->NODE("B", "B")->NODE("C", "C"));
                };
  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);

  ASSERT_EQ(nmda.Init(), SUCCESS);

  const auto b = graph->FindNode("B");
  const auto a = graph->FindNode("A");

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  dlog_setlevel(GE_MODULE_NAME, 2, 0); // 需要warning校验日志
  EXPECT_TRUE(nmda.CanAReuseB(b.get(), 0, a.get(), 0));
  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindWarnLogEndsWith("b cannot find symbol. b: A, b_out_index: 0"), 0);
  dlog_setlevel(GE_MODULE_NAME, 3, 0);
}

TEST_F(UtestDependencyAnalyzer, CanAReuseB_ReturnTrue_WhenAnchorIdNotFound) {
  DEF_GRAPH(g1) {
                  CHAIN(NODE("A", "A")->NODE("B", "B")->NODE("C", "C"));
                };
  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);

  ASSERT_EQ(nmda.Init(), SUCCESS);

  const auto b = graph->FindNode("B");

  DEF_GRAPH(g2) {
                  CHAIN(NODE("D", "D")->NODE("E", "E"));
                };
  auto graph2 = ToGeGraph(g2);
  auto graph22 = ge::GraphUtilsEx::GetComputeGraph(graph2);

  const auto a = graph->FindNode("A");
  const auto d = graph22->FindNode("D");
  ASSERT_NE(d, nullptr);
  ASSERT_NE(a, nullptr);

  gert::GertRuntimeStub stub;
  stub.GetSlogStub().Clear();
  dlog_setlevel(GE_MODULE_NAME, 2, 0); // 需要warning校验日志
  EXPECT_TRUE(nmda.CanAReuseB(d.get(), 0, a.get(), 0));
  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindWarnLogEndsWith("cannot find id for out anchor, node: D, out_index: 0"), 0);
  dlog_setlevel(GE_MODULE_NAME, 3, 0);
}

// ┌────────┐  (1,0)   ┌───┐  (0,0)   ┌───┐  (0,1)   ┌───────────┐
// │   A    │ ───────> │ D │ ───────> │ E │ ───────> │ NetOutput │
// └────────┘          └───┘          └───┘          └───────────┘
//   │                                                 ∧
//   │ (0,0)                                           │
//   ∨                                                 │
// ┌────────┐  (0,0)   ┌───┐  (0,0)                    │
// │   B    │ ───────> │ C │ ──────────────────────────┘
// └────────┘          └───┘
TEST_F(UtestDependencyAnalyzer, ACanReuseB_ButStillCallWhyACannotReuseB) {
  DEF_GRAPH(g1) {
                  CHAIN(NODE("A", "A")->NODE("B", "B")->NODE("C", "C")->NODE("NetOutput", "NetOutput"));
                  CHAIN(NODE("A", "A")->NODE("D", "D")->NODE("E", "E")->NODE("NetOutput", "NetOutput"));
                };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesInvalidStream(graph);

  const auto c = graph->FindNode("C");
  const auto a = graph->FindNode("A");

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  auto reason = nmda.WhyACannotReuseB(c.get(), 0, a.get(), 0);
  EXPECT_STREQ("C_out_0(C, stream:-1, topo:3) can reuse A_out_0(A, stream:-1, topo:0)", reason.c_str());
}

TEST_F(UtestDependencyAnalyzer, GetAllReusableNodesByOutputMemory_Failed_ParamIsNullptr) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("B", "B"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesInvalidStream(graph);

  const auto a = graph->FindNode("A");

  gert::GertRuntimeStub stub;
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  DependencyAnalyzer nmda(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  stub.GetSlogStub().Clear();
  EXPECT_TRUE(GetAllReuableOutAnchors(graph, nmda, nullptr).empty());

  // 接口无法返回error，需要校验日志。
  EXPECT_GE(stub.GetSlogStub().FindErrorLogEndsWith("invalid param."), 0);
}

/*
                         ┌────────┐
  ┌───────────────────── │   E    │
  │                      └────────┘
  │                        ∧
  │                        │ (0,0)
  │                        │
  │  ┌────────┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────┐
  │  │   A    │ ───────> │   B    │ ───────> │ D │ ───────> │ I │ ───────> │ NetOutput │
  │  └────────┘          └────────┘          └───┘          └───┘          └───────────┘
  │    │                                                      ∧
  │    │ (0,0)                                                │
  │    ∨                                                      │
  │  ┌────────┐  (0,0)   ┌────────┐  (0,1)                    │
  │  │   C    │ ───────> │   G    │ ──────────────────────────┘
  │  └────────┘          └────────┘
  │    │
  │    │ (0,0)
  │    ∨
  │  ┌────────┐  (0,1)   ┌────────┐
  │  │   F    │ ───────> │   H    │
  │  └────────┘          └────────┘
  │   (0,0)                ∧
  └────────────────────────┘
 */
TEST_F(UtestDependencyAnalyzer, GetAllReusableNodesByOutputMemory_Success_WithSingleStream) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B"));
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("C", "C"));

    CHAIN(NODE("B", "B")->EDGE(0, 0)->NODE("D", "D")->NODE("I", "I"));
    CHAIN(NODE("B", "B")->EDGE(0, 0)->NODE("E", "E")->NODE("H", "H"));

    CHAIN(NODE("C", "C")->EDGE(0, 0)->NODE("F", "F")->NODE("H", "H")->NODE("NetOutput", "NetOutput"));
    CHAIN(NODE("C", "C")->EDGE(0, 0)->NODE("G", "G")->NODE("I", "I")->NODE("NetOutput", "NetOutput"));
  };

  auto graph1 = ToGeGraph(g1);
  auto graph = ge::GraphUtilsEx::GetComputeGraph(graph1);
  graph->TopologicalSortingGraph();
  SetAllNodesInvalidStream(graph);

  SetStreamForNodes(graph, {"A", "B", "C", "D", "E", "F", "G", "H", "I", "NetOutput"}, 0);

  std::vector<Node *> id_to_node_table(graph->GetAllNodesSize(), nullptr);
  for (const auto &node : graph->GetAllNodes()) {
    const auto id = node->GetOpDescBarePtr()->GetId();
    if (id != -1) {
      id_to_node_table[id] = node.get();
      std::cout << node->GetName() << " : " << id << std::endl;
    }
  }
  ReusalbeMap reusable_map;
  std::vector<NodeNameIndex> nodes_group0 = {{"A", 0}, {"B", 0}, {"C", 0}};
  reusable_map["H"] = nodes_group0;
  std::vector<NodeNameIndex> nodes_group1 = {{"A", 0}, {"B", 0}};
  reusable_map["F"] = nodes_group1;
  reusable_map["G"] = nodes_group1;
  EXPECT_TRUE(CheckAllReusableNodes(graph, reusable_map));
}

/*

                                         sub_1

┌──────────┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────────┐
│ sub_data │ ───────> │   B    │ ───────> │ C │ ───────> │ E │ ───────> │ sub_netoutput │
└──────────┘          └────────┘          └───┘          └───┘          └───────────────┘
                        │                                  ∧
                        │ (0,0)                            │
                        ∨                                  │
                      ┌────────┐  (0,1)                    │
                      │   D    │ ──────────────────────────┘
                      └────────┘

                                              g1

                                 ┌−−−−−−−−−−−−−−−−−−−−−−┐
                                 ╎ partitioned_call:    ╎
                                 ╎                      ╎
┌──────┐  (0,0)   ┌───┐  (0,0)   ╎ ┌──────────────────┐ ╎  (0,0)   ┌───┐  (0,0)   ┌───────────┐
│ data │ ───────> │ A │ ───────> ╎ │ partitioned_call │ ╎ ───────> │ F │ ───────> │ netoutput │
└──────┘          └───┘          ╎ └──────────────────┘ ╎          └───┘          └───────────┘
                                 ╎   │                  ╎
                                 ╎   │                  ╎
                                 ╎   │                  ╎
                                 ╎ ╔ ═ ═ ═ ═ ═ ═ ═ ═ ═╗ ╎
                                 ╎ ∥      sub_1       ∥ ╎
                                 ╎ ╚ ═ ═ ═ ═ ═ ═ ═ ═ ═╝ ╎
                                 ╎                      ╎
                                 └−−−−−−−−−−−−−−−−−−−−−−┘
 */
TEST_F(UtestDependencyAnalyzer, CheckReusalbe_Success_WithOneSubgraph) {
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data)
              ->NODE("B", "B")
              ->EDGE(0, 0)
              ->NODE("C", "C")
              ->NODE("E", "E")
              ->NODE("sub_netoutput", NETOUTPUT));
    CHAIN(NODE("B", "B")->EDGE(0, 0)->NODE("D", "D")->NODE("E", "E"));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("A", "A")
              ->NODE("partitioned_call", PARTITIONEDCALL, sub_1)
              ->NODE("F", "F")
              ->NODE("netoutput", NETOUTPUT));
  };

  sub_1.Layout();
  auto graph = ToComputeGraph(g1);
  NetoutputParentIndexes indexes{{"sub_netoutput", {0}}};
  AddParentIndexForNetoutput(graph, indexes);
  graph->TopologicalSorting();
  SetAllNodesDiffStream(graph);
  SetStreamForNodes(graph, {"A", "F"}, 100);

  std::vector<NodeNameIndex> nodes_group0 = {{"data", 0}, {"sub_data", 0}, {"A", 0}};
  std::vector<NodeNameIndex> nodes_group1 = {{"data", 0}, {"sub_data", 0}, {"A", 0}, {"B", 0}};
  std::vector<NodeNameIndex> nodes_group2 = {{"data", 0}, {"sub_data", 0}, {"A", 0}, {"B", 0}, {"C", 0}, {"D", 0}};

  ReusalbeMap reusable_map{{"C", nodes_group0},
                           {"D", nodes_group0},
                           {"E", nodes_group1},
                           {"partitioned_call", nodes_group1},
                           {"F", nodes_group2}};
  EXPECT_TRUE(CheckAllReusableNodes(graph, reusable_map));
}

/*
                                                     ┌−−−−−−−−−−−−−−−−−−−−−−−┐
                                                     ╎ partitioned_call1:    ╎
                                                     ╎                       ╎
                                                     ╎ ╔ ═ ═ ═ ═ ═ ═ ═ ═ ═ ╗ ╎
                                                     ╎ ∥       sub_1       ∥ ╎
                                                     ╎ ╚ ═ ═ ═ ═ ═ ═ ═ ═ ═ ╝ ╎
                                                     ╎   │                   ╎
                                                     ╎   │                   ╎
                                                     ╎   │                   ╎
┌──────┐  (0,0)     ┌───────────────────┐    (0,0)   ╎ ┌───────────────────┐ ╎  (0,0)   ┌───┐  (0,0)   ┌───────────┐
│ data │ ───────>   │         A         │   ───────> ╎ │ partitioned_call1 │ ╎ ───────> │ F │ ───────> │ netoutput │
└──────┘            └───────────────────┘            ╎ └───────────────────┘ ╎          └───┘          └───────────┘
                                                     ╎                       ╎
                                                     └−−−−−−−−−−−−−−−−−−−−−−−┘
                      │                                                                   ∧
                      │ (0,0)                                                             │
                      ∨                                                                   │
                  ┌−−−−−−−−−−−−−−−−−−−−−−−┐                                               │
                  ╎ partitioned_call2:    ╎                                               │
                  ╎                       ╎                                               │
                  ╎ ┌───────────────────┐ ╎  (0,1)                                        │
                  ╎ │ partitioned_call2 │ ╎ ──────────────────────────────────────────────┘
                  ╎ └───────────────────┘ ╎
                  ╎   │                   ╎
                  ╎   │                   ╎
                  ╎   │                   ╎
                  ╎ ╔ ═ ═ ═ ═ ═ ═ ═ ═ ═ ╗ ╎
                  ╎ ∥       sub_2       ∥ ╎
                  ╎ ╚ ═ ═ ═ ═ ═ ═ ═ ═ ═ ╝ ╎
                  ╎                       ╎
                  └−−−−−−−−−−−−−−−−−−−−−−−┘

                                         sub_1

┌──────────┐  (0,0)   ┌────────┐  (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌───────────────┐
│ sub_data │ ───────> │   B    │ ───────> │ C │ ───────> │ E │ ───────> │ sub_netoutput │
└──────────┘          └────────┘          └───┘          └───┘          └───────────────┘
                        │                                  ∧
                        │ (0,0)                            │
                        ∨                                  │
                      ┌────────┐  (0,1)                    │
                      │   D    │ ──────────────────────────┘
                      └────────┘
topo id:
data : 0
A : 1
partitioned_call1 : 2
sub_data : 3
B : 4
C : 5
D : 6
E : 7
sub1_netoutput : 8
partitioned_call2 : 9
sub2_data : 10
sub2_B : 11
sub2_C : 12
sub2_D : 13
sub2_E : 14
sub2_netoutput : 15
F : 16
netoutput : 17
 */
TEST_F(UtestDependencyAnalyzer, CheckReusalbe_Success_WithTwoSubgraphs) {
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub_1) {
    CHAIN(NODE("sub_data", sub_data)
              ->NODE("B", "B")
              ->EDGE(0, 0)
              ->NODE("C", "C")
              ->NODE("E", "E")
              ->NODE("sub1_netoutput", NETOUTPUT));
    CHAIN(NODE("B", "B")->EDGE(0, 0)->NODE("D", "D")->NODE("E", "E"));
  };
  DEF_GRAPH(sub_2) {
    CHAIN(NODE("sub2_data", sub_data)
              ->NODE("sub2_B", "B")
              ->EDGE(0, 0)
              ->NODE("sub2_C", "C")
              ->NODE("sub2_E", "E")
              ->NODE("sub2_netoutput", NETOUTPUT));
    CHAIN(NODE("sub2_B", "B")->EDGE(0, 0)->NODE("sub2_D", "D")->NODE("sub2_E", "E"));
  };
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)
              ->NODE("A", "A")
              ->NODE("partitioned_call1", PARTITIONEDCALL, sub_1)
              ->NODE("F", "F")
              ->NODE("netoutput", NETOUTPUT));
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("partitioned_call2", PARTITIONEDCALL, sub_2)->NODE("F", "F"));
  };
  auto graph = ToComputeGraph(g1);
  NetoutputParentIndexes indexes{{"sub1_netoutput", {0}}, {"sub2_netoutput", {0}}};
  AddParentIndexForNetoutput(graph, indexes);
  graph->TopologicalSorting();
  SetAllNodesDiffStream(graph);
  SetStreamForNodes(graph, {"A", "partitioned_call1", "partitioned_call2", "F"}, 100);

  std::vector<NodeNameIndex> nodes_group0 = {{"data", 0}};
  std::vector<NodeNameIndex> nodes_group1 = {{"data", 0}, {"B", 0}};
  std::vector<NodeNameIndex> nodes_group2 = {{"data", 0}, {"B", 0}, {"C", 0}, {"D", 0}};
  std::vector<NodeNameIndex> nodes_group3 = {{"data", 0}, {"A", 0},        {"B", 0},        {"C", 0},
                                             {"D", 0},    {"sub_data", 0}, {"sub2_data", 0}};
  std::vector<NodeNameIndex> nodes_group4 = {{"data", 0}, {"A", 0},        {"B", 0},         {"C", 0},
                                             {"D", 0},    {"sub_data", 0}, {"sub2_data", 0}, {"sub2_B", 0}};
  std::vector<NodeNameIndex> nodes_group5 = {{"data", 0},   {"A", 0},        {"B", 0},         {"C", 0},
                                             {"D", 0},      {"sub_data", 0}, {"sub2_data", 0}, {"sub2_B", 0},
                                             {"sub2_C", 0}, {"sub2_D", 0}};
  ReusalbeMap reusable_map{{"B", nodes_group0},
                           {"C", nodes_group0},
                           {"D", nodes_group0},
                           {"E", nodes_group1},
                           {"partitioned_call1", nodes_group1},
                           {"sub2_B", nodes_group2},
                           {"sub2_C", nodes_group3},
                           {"sub2_D", nodes_group3},
                           {"sub2_E", nodes_group4},
                           {"partitioned_call2", nodes_group4},
                           {"F", nodes_group5}};
  EXPECT_TRUE(CheckAllReusableNodes(graph, reusable_map));
}

/*
 *        a
 *      /   \
 *     b     c
 *     |       \
 *     |         d
 *     |        /
 *     |       /
 *     |      /
 *     |     /
 *     |   e
 *       \ |ctrl
 *         f
 *         |
 *         g
 * 纵向在一列的，stream相同，a/e/f/g stream1，b stream2, c stream3, d stream4
 * 校验e复用c，f复用a/c/d
 */
TEST_F(UtestDependencyAnalyzer, CheckReusalbe_Success_MultiStreamReuse) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("A", "A")->NODE("C", "C")->NODE("D", "D")->NODE("E", "E")->Ctrl()->NODE("F", "F"));
    CHAIN(NODE("A", "A")->EDGE(0, 0)->NODE("B", "B")->NODE("F", "F")->NODE("G", "G")->NODE("netoutput", NETOUTPUT));
    CHAIN(NODE("E", "E")->NODE("NetOutput", "NetOutput"));
  };

  auto graph = ToComputeGraph(g1);
  graph->TopologicalSorting();
  SetStreamForNodes(graph, {"A", "E", "F", "G"}, 1);
  SetStreamForNodes(graph, {"B"}, 2);
  SetStreamForNodes(graph, {"C"}, 3);
  SetStreamForNodes(graph, {"D"}, 4);

  std::vector<NodeNameIndex> nodes_group0 = {{"C", 0}};
  std::vector<NodeNameIndex> nodes_group1 = {{"A", 0}, {"C", 0}, {"D", 0}};

  ReusalbeMap reusable_map{{"E", nodes_group0}, {"F", nodes_group1}};
  EXPECT_TRUE(CheckAllReusableNodes(graph, reusable_map));
}

/*
                    data
               +-----+
               |     │      ┌───────────────────────────┐
               |     p1     │ sub1_data                 │
               |     │      │    ┼                      │
               |     │      │    a                      │
               |     │      │    ┼    ┌───────────────┐ │
               |     │      │    p2   │ sub2_data     │ │
               |     │      │    │    │    |          │ │
               |     │      │    │    │    c          │ │
               |     │      │    │    │    ┼          │ │
               |     │      │    │    │ sub2_netoutput│ │
               |     │      │    │    └───────────────┘ │
               |     │      │    b                      │
               |     │      │    ┼                      │
               |     │      │  sub1_netoutput           │
               |     │      └─────────────────────-─────+
               |     |
               |     │   +─────-─────────────────────────┐
               |     p3  │ sub3_data                     │
               |     │   │   ┼                           │
               |     │   │   d                           │
               |     │   │   ┼      ┌──────────────────┐ │
               |     │   │   p4     │ sub4_data        │ │
               |     │   │   │      │   ┼              │ │
               |     │   │   │      │   e              │ │
               |     │   │   │      │   │              │ │
               |     │   │   │      │   f              │ │
               |     │   │   │      │   │              │ │
               |     │   │   │      │   k              │ │
               |     │   │   │      │   ┼              │ │
               |     │   │   │      │ sub4_netoutput   │ │
               |     │   │   │      └──────────────────┘ │
               |     │   │   │      ┌────────────────┐   │
               |     │   │   p5     │ sub5_data      │   │
               |     │   │   │      │    ┼           │   │
               |     │   │   │      │    g           │   │
               |     │   │   │      │    ┼           │   │
               |     │   │   │      │ sub5_netoutput │   │
               |     │   │   │      └────────────────┘   │
               |     │   │ sub3_netoutput                │
               +-----+   └───────────────────────────────┘
                     p6  ┌───────────────┐
                     │   │ sub6_data     │
                     │   │    ┼          │
                     │   │    i          │
                     │   │    ┼          │
                     │   │ sub6_netoutput│
                     │   └───────────────┘
                 netoutput
 */
TEST_F(UtestDependencyAnalyzer, GetReusable_Success_MultiSubGraph) {
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(g1) {
    CHAIN(NODE("data", DATA)->NODE("p1", PARTITIONEDCALL)->NODE("p3", PARTITIONEDCALL)->NODE("p6", PARTITIONEDCALL));
    CHAIN(NODE("data", DATA)->EDGE(0, 1)->NODE("p6", PARTITIONEDCALL)->NODE("netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub1) {
    CHAIN(NODE("sub1_data", sub_data)
              ->NODE("A", "A")
              ->NODE("p2", PARTITIONEDCALL)
              ->NODE("B", "B")
              ->NODE("sub1_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub2) {
    CHAIN(NODE("sub2_data", sub_data)->NODE("C", "C")->NODE("sub2_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub3) {
    CHAIN(NODE("sub3_data", sub_data)
              ->NODE("D", "D")
              ->NODE("p4", PARTITIONEDCALL)
              ->NODE("p5", PARTITIONEDCALL)
              ->NODE("sub3_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub4) {
    CHAIN(
        NODE("sub4_data", sub_data)->NODE("E", "E")->NODE("F", "F")->NODE("K", "K")->NODE("sub4_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub5) {
    CHAIN(NODE("sub5_data", sub_data)->NODE("G", "G")->NODE("sub5_netoutput", NETOUTPUT));
  };
  DEF_GRAPH(sub6) {
    CHAIN(NODE("sub6_data", sub_data)->NODE("I", "I")->NODE("sub6_netoutput", NETOUTPUT));
  };
  auto root_graph = ToComputeGraph(g1);
  auto sub1_graph = ToComputeGraph(sub1);
  auto sub2_graph = ToComputeGraph(sub2);
  auto sub3_graph = ToComputeGraph(sub3);
  auto sub4_graph = ToComputeGraph(sub4);
  auto sub5_graph = ToComputeGraph(sub5);
  auto sub6_graph = ToComputeGraph(sub6);

  auto p1 = root_graph->FindNode("p1");
  auto p3 = root_graph->FindNode("p3");
  auto p6 = root_graph->FindNode("p6");
  auto p2 = sub1_graph->FindNode("p2");
  auto p4 = sub3_graph->FindNode("p4");
  auto p5 = sub3_graph->FindNode("p5");
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(p2, nullptr);
  ASSERT_NE(p3, nullptr);
  ASSERT_NE(p4, nullptr);
  ASSERT_NE(p5, nullptr);
  ASSERT_NE(p6, nullptr);

  sub1_graph->SetParentGraph(root_graph);
  sub3_graph->SetParentGraph(root_graph);
  sub6_graph->SetParentGraph(root_graph);
  sub2_graph->SetParentGraph(sub1_graph);
  sub4_graph->SetParentGraph(sub3_graph);
  sub5_graph->SetParentGraph(sub3_graph);
  root_graph->SetParentGraph(nullptr);

  sub1_graph->SetParentNode(p1);
  sub2_graph->SetParentNode(p2);
  sub3_graph->SetParentNode(p3);
  sub4_graph->SetParentNode(p4);
  sub5_graph->SetParentNode(p5);
  sub6_graph->SetParentNode(p6);

  root_graph->AddSubgraph("sub1_graph", sub1_graph);
  root_graph->AddSubgraph("sub2_graph", sub2_graph);
  root_graph->AddSubgraph("sub3_graph", sub3_graph);
  root_graph->AddSubgraph("sub4_graph", sub4_graph);
  root_graph->AddSubgraph("sub5_graph", sub5_graph);
  root_graph->AddSubgraph("sub6_graph", sub6_graph);

  p1->GetOpDesc()->AddSubgraphName("sub1_graph");
  p2->GetOpDesc()->AddSubgraphName("sub2_graph");
  p3->GetOpDesc()->AddSubgraphName("sub3_graph");
  p4->GetOpDesc()->AddSubgraphName("sub4_graph");
  p5->GetOpDesc()->AddSubgraphName("sub5_graph");
  p6->GetOpDesc()->AddSubgraphName("sub6_graph");

  p1->GetOpDesc()->SetSubgraphInstanceName(0, "sub1_graph");
  p2->GetOpDesc()->SetSubgraphInstanceName(0, "sub2_graph");
  p3->GetOpDesc()->SetSubgraphInstanceName(0, "sub3_graph");
  p4->GetOpDesc()->SetSubgraphInstanceName(0, "sub4_graph");
  p5->GetOpDesc()->SetSubgraphInstanceName(0, "sub5_graph");
  p6->GetOpDesc()->SetSubgraphInstanceName(0, "sub6_graph");

  NetoutputParentIndexes indexes{{"sub1_netoutput", {0}}, {"sub2_netoutput", {0}}, {"sub3_netoutput", {0}},
                                 {"sub4_netoutput", {0}}, {"sub5_netoutput", {0}}, {"sub6_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(root_graph, indexes));
  root_graph->TopologicalSorting();

  std::vector<NodeNameIndex> nodes_group_p1 = {{"A", 0}, {"sub2_data", 0}, {"sub1_data", 0}, {"data", 0}};
  std::vector<NodeNameIndex> nodes_group_p3 = {
      {"F", 0}, {"E", 0},         {"sub4_data", 0}, {"D", 0},    {"sub3_data", 0}, {"C", 0}, {"B", 0},
      {"A", 0}, {"sub2_data", 0}, {"sub1_data", 0}, {"data", 0}, {"p1", 0},        {"p2", 0}};
  std::vector<NodeNameIndex> nodes_group_p4 = {{"E", 0},         {"sub4_data", 0}, {"D", 0},  {"sub3_data", 0},
                                               {"C", 0},         {"B", 0},         {"A", 0},  {"sub2_data", 0},
                                               {"sub1_data", 0}, {"data", 0},      {"p1", 0}, {"p2", 0}};
  std::vector<NodeNameIndex> nodes_group_p6 = {{"F", 0},         {"E", 0},         {"sub4_data", 0}, {"D", 0},
                                               {"sub3_data", 0}, {"C", 0},         {"B", 0},         {"A", 0},
                                               {"sub2_data", 0}, {"sub1_data", 0}, {"data", 0},      {"p4", 0},
                                               {"p1", 0},        {"p2", 0},        {"K", 0},         {"sub5_data", 0}};
  ReusalbeMap reusable_map{{"p1", nodes_group_p1}, {"B", nodes_group_p1},  {"p3", nodes_group_p3},
                           {"p5", nodes_group_p3}, {"G", nodes_group_p3},  {"p4", nodes_group_p4},
                           {"K", nodes_group_p4},  {"p6", nodes_group_p6}, {"I", nodes_group_p6}};
  EXPECT_TRUE(CheckAllReusableNodes(root_graph, reusable_map));
  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(root_graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(root_graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  nmda.Debug();
}

/*
                  ┌−−−−−−−−−−┐
                  ╎ p1:      ╎
                  ╎          ╎
┌──────┐  (0,0)   ╎ ┌──────┐ ╎  (0,0)   ┌───────────┐
│ data │ ───────> ╎ │  p1  │ ╎ ───────> │ netoutput │
└──────┘          ╎ └──────┘ ╎          └───────────┘
                  ╎   │      ╎
                  ╎   │      ╎
                  ╎   │      ╎
                  ╎ ╔ ═ ═ ═╗ ╎
                  ╎ ∥ sub1 ∥ ╎
                  ╎ ╚ ═ ═ ═╝ ╎
                  ╎          ╎
                  └−−−−−−−−−−┘

                                 sub1

                       ┌−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−┐
                       ╎ if:                                          ╎
                       ╎                                              ╎
┌───────────┐  (0,0)   ╎ ┌────────────────┐     ╔ ═ ═ ═╗     ╔ ═ ═ ═╗ ╎
│ sub1_data │ ───────> ╎ │       if       │ ─── ∥ sub2 ∥ ─── ∥ sub3 ∥ ╎
└───────────┘          ╎ └────────────────┘     ╚ ═ ═ ═╝     ╚ ═ ═ ═╝ ╎
                       ╎                                              ╎
                       └−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−┘
                           │
                           │ (0,0)
                           ∨
                         ┌────────────────┐
                         │ sub1_netoutput │
                         └────────────────┘
 */
TEST_F(UtestDependencyAnalyzer, NestingSubgraph) {
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub2) {
                    CHAIN(NODE("sub2_data", sub_data)->NODE("C", "C")->NODE("sub2_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(sub3) {
                    CHAIN(NODE("sub3_data", sub_data)
                              ->NODE("D", "D")
                              ->NODE("sub3_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(sub1) {
                    CHAIN(NODE("sub1_data", sub_data)
                              ->NODE("if", IF, sub2, sub3)
                              ->NODE("sub1_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(g1) {
                  CHAIN(NODE("data", DATA)->NODE("p1", PARTITIONEDCALL, sub1)->NODE("netoutput", NETOUTPUT));
                };

  auto root_graph = ToComputeGraph(g1);
  auto sub1_graph = ToComputeGraph(sub1);
  auto sub2_graph = ToComputeGraph(sub2);
  auto sub3_graph = ToComputeGraph(sub3);

  auto p1 = root_graph->FindNode("p1");
  auto if_parent = sub1_graph->FindNode("if");
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(if_parent, nullptr);

  sub1_graph->SetParentGraph(root_graph);
  sub2_graph->SetParentGraph(sub1_graph);
  sub3_graph->SetParentGraph(sub1_graph);
  root_graph->SetParentGraph(nullptr);

  sub1_graph->SetParentNode(p1);
  sub2_graph->SetParentNode(if_parent);
  sub3_graph->SetParentNode(if_parent);

  root_graph->AddSubgraph("sub1_graph", sub1_graph);
  root_graph->AddSubgraph("sub2_graph", sub2_graph);
  root_graph->AddSubgraph("sub3_graph", sub3_graph);

  p1->GetOpDesc()->AddSubgraphName("sub1_graph");
  if_parent->GetOpDesc()->AddSubgraphName("sub2_graph");
  if_parent->GetOpDesc()->AddSubgraphName("sub3_graph");

  p1->GetOpDesc()->SetSubgraphInstanceName(0, "sub1_graph");
  if_parent->GetOpDesc()->SetSubgraphInstanceName(0, "sub2_graph");
  if_parent->GetOpDesc()->SetSubgraphInstanceName(1, "sub3_graph");

  NetoutputParentIndexes indexes{{"sub1_netoutput", {0}}, {"sub2_netoutput", {0}}, {"sub3_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(root_graph, indexes));
  root_graph->TopologicalSorting();

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(root_graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(root_graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();
  nmda.Debug();
}

/*
                          g1

                  ┌−−−−−−−−−−┐
                  ╎ p1:      ╎
                  ╎          ╎
┌──────┐  (0,0)   ╎ ┌──────┐ ╎  (0,0)   ┌───────────┐
│ data │ ───────> ╎ │  p1  │ ╎ ───────> │ netoutput │
└──────┘          ╎ └──────┘ ╎          └───────────┘
                  ╎   │      ╎
                  ╎   │      ╎
                  ╎   │      ╎
                  ╎ ╔ ═ ═ ═╗ ╎
                  ╎ ∥ sub1 ∥ ╎
                  ╎ ╚ ═ ═ ═╝ ╎
                  ╎          ╎
                  └−−−−−−−−−−┘

                                                      sub1

                       ┌−−−−−−−−−−┐
                       ╎ if:      ╎
                       ╎          ╎
┌───────────┐  (0,0)   ╎ ┌──────┐ ╎  (0,0)   ┌──────┐    (0,0)   ┌───┐  (0,0)   ┌───┐  (0,0)   ┌────────────────┐
│ sub1_data │ ───────> ╎ │  if  │ ╎ ───────> │  E   │   ───────> │ F │ ───────> │ G │ ───────> │ sub1_netoutput │
└───────────┘          ╎ └──────┘ ╎          └──────┘            └───┘          └───┘          └────────────────┘
                       ╎   │      ╎
                       ╎   │      ╎
                       ╎   │      ╎
                       ╎           −−−−−−−−−−−−−−−−−−−┐
                       ╎                              ╎
                       ╎ ╔ ═ ═ ═╗            ╔ ═ ═ ═╗ ╎
                       ╎ ∥ sub2 ∥   ──────── ∥ sub3 ∥ ╎
                       ╎ ╚ ═ ═ ═╝            ╚ ═ ═ ═╝ ╎
                       ╎                              ╎
                       └−−−−−−−−−−−−−−−−−−−−−−−−−−−−−−┘

                         sub2

┌───────────┐  (0,0)   ┌───┐  (0,0)   ┌────────────────┐
│ sub2_data │ ───────> │ C │ ───────> │ sub2_netoutput │
└───────────┘          └───┘          └────────────────┘

                         sub3

┌───────────┐  (0,0)   ┌───┐  (0,0)   ┌────────────────┐
│ sub3_data │ ───────> │ D │ ───────> │ sub3_netoutput │
└───────────┘          └───┘          └────────────────┘
 这里要看护的场景时，动态shape静态子图的场景下，顶层静态子图中的父节点没有放到parents_on_root_graph_，
 导致ExtendReachNodesMapCrossSubGraph没有对跨子图的reach table做扩展
 */
TEST_F(UtestDependencyAnalyzer, NestingSubgraph_KnownSubgraph) {
  const auto sub_data = OP_CFG(DATA).ParentNodeIndex(0);
  DEF_GRAPH(sub2) {
                    CHAIN(NODE("sub2_data", sub_data)->NODE("C", "C")->NODE("sub2_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(sub3) {
                    CHAIN(NODE("sub3_data", sub_data)
                              ->NODE("D", "D")
                              ->NODE("sub3_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(sub1) {
                    CHAIN(NODE("sub1_data", sub_data)
                              ->NODE("if", IF, sub2, sub3)->NODE("E", CAST)->NODE("F", CAST)->NODE("G", CAST)
                              ->NODE("sub1_netoutput", NETOUTPUT));
                  };
  DEF_GRAPH(g1) {
                  CHAIN(NODE("data", DATA)->NODE("p1", PARTITIONEDCALL, sub1)->NODE("netoutput", NETOUTPUT));
                };

  auto root_graph = ToComputeGraph(g1);
  auto sub1_graph = ToComputeGraph(sub1);
  auto sub2_graph = ToComputeGraph(sub2);
  auto sub3_graph = ToComputeGraph(sub3);

  auto p1 = root_graph->FindNode("p1");
  auto if_parent = sub1_graph->FindNode("if");
  ASSERT_NE(p1, nullptr);
  ASSERT_NE(if_parent, nullptr);

  sub1_graph->SetParentGraph(root_graph);
  sub2_graph->SetParentGraph(sub1_graph);
  sub3_graph->SetParentGraph(sub1_graph);
  root_graph->SetParentGraph(nullptr);

  sub1_graph->SetParentNode(p1);
  sub2_graph->SetParentNode(if_parent);
  sub3_graph->SetParentNode(if_parent);

  root_graph->AddSubgraph("sub1_graph", sub1_graph);
  root_graph->AddSubgraph("sub2_graph", sub2_graph);
  root_graph->AddSubgraph("sub3_graph", sub3_graph);

  p1->GetOpDesc()->AddSubgraphName("sub1_graph");
  if_parent->GetOpDesc()->AddSubgraphName("sub2_graph");
  if_parent->GetOpDesc()->AddSubgraphName("sub3_graph");

  p1->GetOpDesc()->SetSubgraphInstanceName(0, "sub1_graph");
  if_parent->GetOpDesc()->SetSubgraphInstanceName(0, "sub2_graph");
  if_parent->GetOpDesc()->SetSubgraphInstanceName(1, "sub3_graph");

  NetoutputParentIndexes indexes{{"sub1_netoutput", {0}}, {"sub2_netoutput", {0}}, {"sub3_netoutput", {0}}};
  ASSERT_TRUE(AddParentIndexForNetoutput(root_graph, indexes));
  root_graph->TopologicalSorting();

  // 注意不是root_graph, sub1_graph is known subgraph
  sub1_graph->TopologicalSorting();

  std::vector<NodeNameIndex> nodes_group0 = {{"D"}, {"sub3_data"}, {"C"}, {"sub2_data"}, {"if"}, {"sub1_data"}};
  ReusalbeMap reusable_map{{"F", nodes_group0}};
  EXPECT_TRUE(CheckAllReusableNodes(sub1_graph, reusable_map));
}

/*
 *      root graph
 *         data1(0)
 *          |
 *          a(1)
 *          |            ffts+ sub graph
 *   partitioned_call(2)  +------------+
 *          |             |  data2(3)  |
 *          e(7)          |   /   \    |
 *          |             |  b(4)  |ctrl|
 *        netoutput(8)    |       /    |
 *                        |    c(5)    |
 *                        |    |       |
 *                        |Netoutput(6)|
 *                        +------------+
 *  GetReachNodesBitmapIntersection 中，当输出节点为空时，该节点被任何b到达的点复用
 *  但是这里b不能被复用
 *  b能达到partitioned_call，也就是被partitioned_call复用，但是b和c是并行的, 不能被c复用，也就不能被partitioned-call复用
 */
TEST_F(UtestDependencyAnalyzer, CheckReusable_Success_NoOutputNodeNotBeReused) {
  auto graph = block_mem_ut::BuildGraphForNetoutputNotReuseData();
  graph->TopologicalSorting();
  SetAllNodesDiffStream(graph);
  std::vector<NodeNameIndex> nodes_group0 = {{"data1"}};
  ReusalbeMap reusable_map{{"c", nodes_group0}};
  EXPECT_TRUE(CheckAllReusableNodes(graph, reusable_map));
}

//
//     A:0    B:1
//      \  /
//     stream_merge1:2(s:0) (two out data anchor, but only has one output node)
//        |
//        C:3(s:0)  D:4(s:1)
//        \         /
//     stream_merge2:5(s:2) (C D output use same symbol)
//          |
//       netoutput:6
//
//
TEST_F(UtestDependencyAnalyzer, StreamMergeHasTwoOutDataAnchorButOnlyHasOneOutputNode_ReusedWithDirectConnectNode) {
  auto graph = block_mem_ut::BuildGraphWithStreamMerge();
  graph->TopologicalSorting();

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();

  std::unordered_map<std::string, NodePtr> name_to_node;
  for (const auto &node : graph->GetAllNodes()) {
    name_to_node[node->GetName()] = node;
    std::cout << node->GetName() << ", topoid: " << node->GetOpDescBarePtr()->GetId() << std::endl;
  }

  ASSERT_FALSE(nmda.CanAReuseB(name_to_node["C"].get(), 0U, name_to_node["stream_merge1"].get(), 1U));
  nmda.WhyACannotReuseB(name_to_node["C"].get(), 0U, name_to_node["stream_merge1"].get(), 1U);
}

/*
 *      root graph
 *         data1(0)
 *          |
 *          a(1)
 *          |            ffts+ sub graph
 *   partitioned_call(2)  +------------+
 *          |             |  data2(3)  |
 *          e(7)          |   /   \    |
 *          |             |  b(4)  |ctrl|
 *        netoutput(8)    |       /    |
 *                        |    c(5)    |
 *                        |    |       |
 *                        |Netoutput(6)|
 *                        +------------+
 *  GetReachNodesBitmapIntersection 中，当输出节点为空时，该节点被任何b到达的点复用
 *  但是这里b不能被复用
 *  b能达到partitioned_call，也就是被partitioned_call复用，但是b和c是并行的, 不能被c复用，也就不能被partitioned-call复用
 */
TEST_F(UtestDependencyAnalyzer, Debug_ReRunInitReachTable) {
  auto graph = block_mem_ut::BuildGraphForNetoutputNotReuseData();
  graph->TopologicalSorting();

  SymbolToAnchors symbol_to_anchors;
  AnchorToSymbol anchor_to_symbol;
  auto ret = GraphUtils::GetRefMapping(graph, symbol_to_anchors, anchor_to_symbol);
  EXPECT_EQ(ret, GRAPH_SUCCESS);

  DependencyAnalyzer nmda = DependencyAnalyzer(graph, anchor_to_symbol, symbol_to_anchors);
  nmda.Init();

  std::unordered_map<std::string, NodePtr> name_to_node;
  for (const auto &node : graph->GetAllNodes()) {
    name_to_node[node->GetName()] = node;
  }
  // 有些代码必须开info日志才能走到
  dlog_setlevel(GE_MODULE_NAME, 1, 0);
  nmda.Debug();
  dlog_setlevel(GE_MODULE_NAME, 3, 0);
}
}  // namespace ge