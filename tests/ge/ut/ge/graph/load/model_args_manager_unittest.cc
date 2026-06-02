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
#include <memory>
#include "graph/load/model_manager/model_args_manager.h"
#include "graph/load/model_manager/davinci_model.h"
#include "graph/load/model_manager/task_info/task_info.h"

#include "common/share_graph.h"
#include "faker/active_mem_base_faker.h"
#include "faker/aicore_taskdef_faker.h"
#include "faker/dsacore_taskdef_faker.h"
#include "faker/ge_model_builder.h"
#include "faker/label_switch_task_def_faker.h"
#include "faker/rts_task_def_faker.h"
#include "faker/event_record_task_def_faker.h"
#include "fixed_addr_bulk_checker.h"
#include "stub/gert_runtime_stub.h"
#include "task_args_refresh_type_classifier_common_header.h"
#include "task_info_init_checker.h"
#include "task_info_stubs.h"
#include "common/opskernel/ops_kernel_info_types.h"

namespace ge {
namespace {
void InsertStubAllocator(DavinciModel *model) {
  model->mem_type_to_allocator_[RT_MEMORY_HBM] = MakeShared<StubMemoryBlockManager>(RT_MEMORY_HBM);
  model->mem_type_to_allocator_[RT_MEMORY_TS] = MakeShared<StubMemoryBlockManager>(RT_MEMORY_TS);
  model->mem_type_to_allocator_[RT_MEMORY_DDR] = MakeShared<StubMemoryBlockManager>(RT_MEMORY_DDR);
  model->mem_type_to_allocator_[RT_MEMORY_DEFAULT] = MakeShared<StubMemoryBlockManager>(RT_MEMORY_DEFAULT);
}

StubMemoryBlockManager *GetStubAllocator(DavinciModel *model, ArgsPlacement placement) {
  auto rt_mem_type = ge::GetRtsMemoryType(placement, 0);
  auto iter = model->mem_type_to_allocator_.find(rt_mem_type);
  if (iter != model->mem_type_to_allocator_.end()) {
    std::shared_ptr<MemoryBlockManager> allocator = iter->second;
    return dynamic_cast<StubMemoryBlockManager *>(allocator.get());
  } else {
    return nullptr;
  }
}

const gert::RuntimeStubImpl::MemoryInfo *FindMemoryInfo(const gert::GertRuntimeStub &runtime_stub,
                                                        StubMemoryBlockManager *allocator,
                                                        const std::vector<ModelArgsManager::ModelArgs> &model_args,
                                                        ArgsPlacement placement) {
  for (const auto &arg : model_args) {
    if (arg.placement == placement) {
      if (allocator != nullptr) {
        auto &addrs_to_allocated_mem = allocator->GetAllocatedRtsMemory();
        auto iter = addrs_to_allocated_mem.find(ValueToPtr(arg.model_args_device_addr));
        if (iter == addrs_to_allocated_mem.end()) {
          std::cerr << "find memory info failed, addr: " << arg.model_args_device_addr << std::endl;
          return nullptr;
        }
        return &iter->second;
      } else {
        auto &addrs_to_allocated_mem = runtime_stub.GetRtsRuntimeStub().GetAllocatedRtsMemory();
        auto iter = addrs_to_allocated_mem.find(ValueToPtr(arg.model_args_device_addr));
        return &iter->second;
      }
    }
  }
  return nullptr;
}
bool ModelArgsHasPlacement(const gert::GertRuntimeStub &runtime_stub, StubMemoryBlockManager *allocator,
                           const std::vector<ModelArgsManager::ModelArgs> &model_args, ArgsPlacement placement,
                           std::string &ret) {
  std::array<rtMemType_t, static_cast<int32_t>(ArgsPlacement::kEnd)> expect_placements = {
      RT_MEMORY_HBM,  // hbm
      RT_MEMORY_TS,   // ts
      RT_MEMORY_HBM,  // sqe
      RT_MEMORY_HOST_SVM
  };
  std::stringstream ss;
  for (const auto &arg : model_args) {
    gert::RuntimeStubImpl::MemoryInfo mem_info;
    if (arg.placement == placement) {
      if (allocator != nullptr) {
        auto &addrs_to_allocated_mem = allocator->GetAllocatedRtsMemory();
        auto iter = addrs_to_allocated_mem.find(ValueToPtr(arg.model_args_device_addr));
        if (iter == addrs_to_allocated_mem.end()) {
          ss << "The device addr " << std::hex << arg.model_args_device_addr << " of placement "
             << GetArgsPlacementStr(placement) << " does not exists in allocator" << std::endl;
          ret = ss.str();
          return false;
        }
        mem_info = iter->second;
      } else {
        auto &addrs_to_allocated_mem = runtime_stub.GetRtsRuntimeStub().GetAllocatedRtsMemory();
        auto iter = addrs_to_allocated_mem.find(ValueToPtr(arg.model_args_device_addr));
        if (iter == addrs_to_allocated_mem.end()) {
          ss << "The device addr " << std::hex << arg.model_args_device_addr << " of placement "
             << GetArgsPlacementStr(placement) << " does not exists" << std::endl;
          ret = ss.str();
          return false;
        }
        mem_info = iter->second;
      }
      if (mem_info.rts_mem_type != expect_placements[static_cast<int32_t>(arg.placement)]) {
        ss << "Invalid rts memory type " << std::hex << mem_info.rts_mem_type << ", expect " << std::hex
           << expect_placements[static_cast<int32_t>(arg.placement)] << ", placement "
           << GetArgsPlacementStr(arg.placement) << std::endl;
        ret = ss.str();
        return false;
      }
      return true;
    }
  }
  ss << "Placement " << GetArgsPlacementStr(placement) << " does not in model args" << std::endl;
  ret = ss.str();
  return false;
}
std::unordered_map<std::string, std::vector<size_t>> ConvertToNoeKey(
    const ComputeGraphPtr &graph, const std::unordered_map<size_t, int64_t> &task_indexes_to_node_id) {
  std::unordered_map<int64_t, Node *> node_ids_to_node;
  for (const auto &node : graph->GetAllNodes()) {
    node_ids_to_node[node->GetOpDesc()->GetId()] = node.get();
  }

  std::unordered_map<std::string, std::vector<size_t>> node_names_to_task_indexes;
  for (const auto &task_index_and_node_id : task_indexes_to_node_id) {
    std::string node_name = "-1";
    if (task_index_and_node_id.second != -1) {
      node_name = node_ids_to_node.at(task_index_and_node_id.second)->GetName();
    }
    node_names_to_task_indexes[node_name].push_back(task_index_and_node_id.first);
  }
  return node_names_to_task_indexes;
}

ge::NodePtr BuildTestNode() {
  auto graph = std::make_shared<ge::ComputeGraph>("graph");
  auto op_desc = std::make_shared<ge::OpDesc>("node", "node");
  return graph->AddNode(op_desc);
}

#define GET_TASK_INFO(node_name) \
  reinterpret_cast<StubTaskInfo *>(task_list[node_names_to_task_indexes.at(node_name).at(0)].get())
#define GET_UPDATE_CALL_TIMES(node_name) GET_TASK_INFO(node_name)->GetUpdateDumpInfosCalls().size()
#define CLEAR_ALL_TASK_INFO(tl)                                   \
  do {                                                            \
    for (auto &task_info : tl) {                                  \
      reinterpret_cast<StubTaskInfo *>(task_info.get())->Clear(); \
    }                                                             \
  } while (0)

}  // namespace

class MockTaskInfoWithAllocResults : public TaskInfo {
 public:
  explicit MockTaskInfoWithAllocResults(const std::vector<ArgsAllocationResult> &results)
      : results_(results) {}
  Status Init(const domi::TaskDef &, DavinciModel *, const PisToArgs &, const PisToPersistentWorkspace &,
              const IowAddrs &) override { return SUCCESS; }
  Status Distribute() override { return SUCCESS; }
  Status Release() override { return SUCCESS; }
  int64_t ParseOpIndex(const domi::TaskDef &) const override { return 0; }
  Status ParseTaskRunParam(const domi::TaskDef &, DavinciModel *, TaskRunParam &) override { return SUCCESS; }
  const std::vector<ArgsAllocationResult>& GetArgsAllocationResults() const override { return results_; }
 private:
  std::vector<ArgsAllocationResult> results_;
};

class MockCustomOpTaskInfo : public TaskInfo {
 public:
  explicit MockCustomOpTaskInfo() : update_host_args_void_calls_(0) {}
  Status Init(const domi::TaskDef &, DavinciModel *, const PisToArgs &, const PisToPersistentWorkspace &,
              const IowAddrs &) override { return SUCCESS; }
  Status Distribute() override { return SUCCESS; }
  Status Release() override { return SUCCESS; }
  int64_t ParseOpIndex(const domi::TaskDef &) const override { return 0; }
  Status ParseTaskRunParam(const domi::TaskDef &, DavinciModel *, TaskRunParam &) override { return SUCCESS; }
  Status UpdateHostArgs(void *base_addr, size_t mem_size) override {
    (void)base_addr;
    (void)mem_size;
    update_host_args_void_calls_++;
    return SUCCESS;
  }
  bool NeedReserveArgsTable() const override { return true; }
  int update_host_args_void_calls_;
};

class ModelArgsManagerUT : public testing::Test {};
/**
 * 预置条件：
 * 1. feature map refreshable: true
 */
TEST_F(ModelArgsManagerUT, InitV2_TaskInitParametersCorrect_AllRefreshable) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);

  ASSERT_EQ(task_list.size(), 2UL);

  auto checker = TASK_INFO_CHECKER(task_list, 0)
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(),
                                                ArgsPlacement::kArgsPlacementHbm));
  std::string ret;
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, 1)
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(),
                                           ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeFeatureMap, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
}
/**
 * 预置条件：
 * 1. feature map refreshable: false
 */
TEST_F(ModelArgsManagerUT, InitV2_TaskInitParametersCorrect_AllRefreshableAndFmNotRfreshable) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);

  ASSERT_EQ(task_list.size(), 2UL);

  auto checker = TASK_INFO_CHECKER(task_list, 0)
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(),
                                                ArgsPlacement::kArgsPlacementHbm));
  std::string ret;
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, 1)
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(),
                                           ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeFeatureMap, false}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
}
/**
 * 预置条件：
 * 1. feature map refreshable: false
 */
TEST_F(ModelArgsManagerUT, InitV2_TaskInitParametersCorrect_HbmTsSqeArgs) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetSlogStub().SetLevelDebug();
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  auto ts_allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementTs);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  auto &model_args = mam.GetModelArgs();
  ASSERT_EQ(model_args.size(), 2UL);
  std::string ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, allocator, model_args, ArgsPlacement::kArgsPlacementHbm, ret)) << ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, ts_allocator,
                                    model_args, ArgsPlacement::kArgsPlacementTs, ret)) << ret;

  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);

  ASSERT_EQ(task_list.size(), 4UL);
  auto checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add1").at(0))
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(),
                                                ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add2").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}, {MemoryAppType::kMemoryTypeFeatureMap, false}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;

  checker =
      TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("dsa1").at(0))
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
          // 虽然返回的有一块sqe内存，不过sqe内存在申请的时候是合并到hbm中一起申请的，因此从rts视角，看到的仍然是hbm内存
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator, mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;


  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("id1").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, ts_allocator, mam.GetModelArgs(),
                                           ArgsPlacement::kArgsPlacementTs));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;
}

/**
 * 预置条件：
 * 1. feature map refreshable: true
 */
TEST_F(ModelArgsManagerUT, InitV2_FixedAddrCorrect_NotSupportRefreshConnectToRefreshableFm) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  auto ts_allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementTs);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  auto &model_args = mam.GetModelArgs();
  ASSERT_EQ(model_args.size(), 2UL);
  std::string ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, allocator, model_args, ArgsPlacement::kArgsPlacementHbm, ret)) << ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, ts_allocator,
                                    model_args, ArgsPlacement::kArgsPlacementTs, ret)) << ret;

  auto &fixed_addr = mam.GetFixedAddrBulk();
  ASSERT_TRUE(FixedAddrBulkChecker(runtime_stub, ts_allocator, fixed_addr, node_names_to_task_indexes)
                  .FixedMemory({{"dsa1", TaskArgsRefreshTypeClassifier::kOutput, 0},
                                {"id1", TaskArgsRefreshTypeClassifier::kInput, 0}})
                  .Check(ret))
      << ret;

  ASSERT_EQ(task_list.size(), 4UL);
  auto checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add1").at(0))
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                                mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
                     .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add2").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
                .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}, {MemoryAppType::kMemoryTypeFeatureMap, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker =
      TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("dsa1").at(0))
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                     mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
          // 虽然返回的有一块sqe内存，不过sqe内存在申请的时候是合并到hbm中一起申请的，因此从rts视角，看到的仍然是hbm内存
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                     mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
          .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFix, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;
  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("id1").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, ts_allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementTs))
                .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {{MemoryAppType::kMemoryTypeFix, false}})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;
}
TEST_F(ModelArgsManagerUT, InitV2_FixedAddrReplacedCorrectly_LabelSwitchTaskHasNoArgsAndHasFixedAddr) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<RtsLabelSwitchStubTaskInfo>(ModelTaskType::MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildGraphHasLabelSwitch();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .AddTaskDef("ls1", gert::LabelSwitchTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  auto ts_allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementTs);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  auto &model_args = mam.GetModelArgs();
  ASSERT_EQ(model_args.size(), 2UL);
  std::string ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, allocator,
                                    model_args, ArgsPlacement::kArgsPlacementHbm, ret)) << ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, ts_allocator,
                                    model_args, ArgsPlacement::kArgsPlacementTs, ret)) << ret;

  auto &fixed_addr = mam.GetFixedAddrBulk();
  ASSERT_TRUE(FixedAddrBulkChecker(runtime_stub, ts_allocator, fixed_addr, node_names_to_task_indexes)
                  .FixedMemory({{"id1", TaskArgsRefreshTypeClassifier::kOutput, 0},
                                {"ls1", TaskArgsRefreshTypeClassifier::kInput, 0}})
                  .FixedMemory({{"ls1", TaskArgsRefreshTypeClassifier::kWorkspace, 0}})
                  .Check(ret))
      << ret;

  ASSERT_EQ(task_list.size(), 4UL);
  auto checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add1").at(0))
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                                mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
                     .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add2").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
                .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeFeatureMap, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("ls1").at(0)).FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {{MemoryAppType::kMemoryTypeFix, false}})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {{MemoryAppType::kMemoryTypeFix, false}})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("id1").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, ts_allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementTs))
                .FixedAddrBulk(fixed_addr);
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, true}})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFix, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;
}
TEST_F(ModelArgsManagerUT, InitV2_AllOthersCorrect_EventTaskHasNoCorrospondingNode) {
  gert::GertRuntimeStub runtime_stub;
  dlog_setlevel(GE_MODULE_NAME, 0, 0);
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<EventStubTaskInfo>(ModelTaskType::MODEL_TASK_EVENT_RECORD)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .AppendTaskDef(gert::EventRecordTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  InsertStubAllocator(davinci_model.get());
  auto allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementHbm);
  auto ts_allocator = GetStubAllocator(davinci_model.get(), ArgsPlacement::kArgsPlacementTs);
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  auto &model_args = mam.GetModelArgs();
  ASSERT_EQ(model_args.size(), 2UL);
  std::string ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, allocator, model_args, ArgsPlacement::kArgsPlacementHbm, ret)) << ret;
  EXPECT_TRUE(ModelArgsHasPlacement(runtime_stub, ts_allocator,
                                    model_args, ArgsPlacement::kArgsPlacementTs, ret)) << ret;

  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);

  ASSERT_EQ(task_list.size(), 5UL);
  auto checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add1").at(0))
                     .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                                mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeModelIo, true}, {MemoryAppType::kMemoryTypeModelIo, true}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("add2").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(
      ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}, {MemoryAppType::kMemoryTypeFeatureMap, false}}))
      << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;

  checker =
      TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("dsa1").at(0))
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                     mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm))
          // 虽然返回的有一块sqe内存，不过sqe内存在申请的时候是合并到hbm中一起申请的，因此从rts视角，看到的仍然是hbm内存
          .ArgsMemory(FindMemoryInfo(runtime_stub, allocator,
                                     mam.GetModelArgs(), ArgsPlacement::kArgsPlacementHbm));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;

  checker = TASK_INFO_CHECKER(task_list, node_names_to_task_indexes.at("id1").at(0))
                .ArgsMemory(FindMemoryInfo(runtime_stub, ts_allocator,
                                           mam.GetModelArgs(), ArgsPlacement::kArgsPlacementTs));
  ASSERT_TRUE(checker.GeneralCheck(ret)) << ret;
  ASSERT_TRUE(checker.CheckInputAddrs(ret, {{MemoryAppType::kMemoryTypeFeatureMap, false}})) << ret;
  ASSERT_TRUE(checker.CheckOutputAddrs(ret, {{MemoryAppType::kMemoryTypeModelIo, true}})) << ret;
  ASSERT_TRUE(checker.CheckWsAddrs(ret, {})) << ret;
}
TEST_F(ModelArgsManagerUT, InitV2_Suceess_NoTaskInModelDef) {
  domi::ModelTaskDef model_task_def;
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(model_task_def, &task_list), SUCCESS);
}
/**
 * 预置条件：
 * 1. feature map refreshable: false
 */
TEST_F(ModelArgsManagerUT, UpdateForExecute_AllTaskInfoCalled_FirstTime) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  mam.SetAllocationHitCount(1U, 1U);
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  for (const auto &task_info : task_list) {
    ASSERT_EQ(reinterpret_cast<StubTaskInfo *>(task_info.get())->GetUpdateDumpInfosCalls().size(), 1UL);
  }
  // todo 所有H2D的args拷贝地址、长度正确
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_AllTaskInfoCalled_FirstTime_With_UpdateModelParamOp) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  mam.SetAllocationHitCount(1U, 1U);
  std::string stub_func_str = "func_test";
  mam.SetFuncHandle(static_cast<void*>(const_cast<char*>(stub_func_str.c_str())));
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  // todo 所有H2D的args拷贝地址、长度正确
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_AllTaskInfoCalled_FirstTime_Version_1) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  mam.SetAllocationHitCount(1U, 1U);
  mam.update_version_ = 1U;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }
  mam.update_version_ = 1;
  uint32_t up = 1;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  for (const auto &task_info : task_list) {
    ASSERT_EQ(reinterpret_cast<StubTaskInfo *>(task_info.get())->GetUpdateHostArgsCalls().size(), 1UL);
  }
  // todo 所有H2D的args拷贝地址、长度正确
}

// fusion段更新testcase
TEST_F(ModelArgsManagerUT, UpdateForExecute_AllTaskInfoCalled_FusionChanged) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);
  std::vector<int64_t> fusion_lengths = {0x100};
  std::vector<int64_t> fm_lengths = {0x100};
  auto davinci_model = DavinciModelFaker()
                           .ModelFusionLengths(fusion_lengths)
                           .ModelFmLengths(fm_lengths)
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 1U);
  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(fusion_lengths.size(), fm_lengths.size(), 2, 1)
                                 .FusionBaseIndex(0).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t ret_up = 4;
  ASSERT_EQ(mam.UpdateForExecute(ret_up, stream),
            SUCCESS);

  for (const auto &task_info : task_list) {
    ASSERT_EQ(reinterpret_cast<StubTaskInfo *>(task_info.get())->GetUpdateDumpInfosCalls().size(), 1UL);
  }
  // fm base changed
  ret_up = 2;
  ASSERT_EQ(mam.UpdateForExecute(ret_up, stream),
            SUCCESS);

  // add1 will be updated in policies model-io,fm-and-model-io,all-one-time
  // add2 will be updated in policies model-io,fm-and-model-io,all-one-time
  // id1 will be updated in policies model-io,fm-and-model-io,all-one-time
  // dsa1 will be updated in policies all-one-time,
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add1"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add2"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("id1"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("dsa1"), 1UL);
}

// FM分段申请更新testcase
TEST_F(ModelArgsManagerUT, UpdateForExecute_AllTaskInfoCalled_SubFmChanged) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);
  std::vector<int64_t> fm_lengths = {0x100, 0x100};  // FM分段申请
  auto davinci_model = DavinciModelFaker()
                           .ModelFmLengths(fm_lengths)
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr1 = ActiveMemBaseFaker(fm_lengths.size(), 2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr1.size());
  uint64_t* active_mem_base_addr_temp1 = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr1.size(); i++) {
    active_mem_base_addr_temp1[i] = active_mem_base_addr1[i];
  }

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  for (const auto &task_info : task_list) {
    ASSERT_EQ(reinterpret_cast<StubTaskInfo *>(task_info.get())->GetUpdateDumpInfosCalls().size(), 1UL);
  }
  // fm base changed
  std::vector<uint64_t> active_mem_base_addr2 = ActiveMemBaseFaker(fm_lengths.size(), 2, 1).FmBaseIndex(1).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr2.size());
  uint64_t* active_mem_base_addr_temp2 = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr2.size(); i++) {
    active_mem_base_addr_temp2[i] = active_mem_base_addr2[i];
  }

  up = 3;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  // 2 意味着被刷新了两次， 也就是这个算子的地址含有fm的地址
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add1"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add2"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("id1"), 2UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("dsa1"), 1UL);
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_OnlyModelIoCalled_IoChanged) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  dlog_setlevel(0,0,0);
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 1U);
  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  mam.SetFuncHandle((void*)100);

  uint32_t up = 2;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  // model io addr changed
  CLEAR_ALL_TASK_INFO(task_list);
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add1"), 1UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add2"), 1UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("id1"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("dsa1"), 0UL);

  // model io and fm does not change
  CLEAR_ALL_TASK_INFO(task_list);

  up = 0;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add1"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add2"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("id1"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("dsa1"), 0UL);

  // version == 1
  mam.update_version_ = 1U;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  // version == 3
  mam.update_version_ = 3U;
  up = 2;
  gert::GlobalProfilingWrapper::GetInstance()->SetEnableFlags(
      gert::BuiltInSubscriberUtil::BuildEnableFlags<gert::ProfilingType>(
          {gert::ProfilingType::kTaskTime}));
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  gert::GlobalProfilingWrapper::GetInstance()->SetEnableFlags(
      gert::BuiltInSubscriberUtil::BuildEnableFlags<gert::ProfilingType>(
          {gert::ProfilingType::kTaskTime, gert::ProfilingType::kDevice}));
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  gert::GlobalProfilingWrapper::GetInstance()->SetEnableFlags(0);

  // print dfx info
  mam.dfx_info_.get_model_args_device_table_flag = true;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  up = 0;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  dlog_setlevel(0,3,0);
}


TEST_F(ModelArgsManagerUT, UpdateForExecute_OnlyModelIoCalled_IoChanged_NoModelIoHit) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  std::set<std::string> temp;
  davinci_model.get()->data_dumper_.dump_properties_.model_dump_properties_map_.emplace(DUMP_ALL_MODEL, temp);

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 0U);
  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 0;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  // model io addr changed and no model io hit
  CLEAR_ALL_TASK_INFO(task_list);
  std::vector<uint64_t> active_mem_base_addr1 = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(2).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr1.size());
  uint64_t* active_mem_base_addr_temp1 = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr1.size(); i++) {
    active_mem_base_addr_temp1[i] = active_mem_base_addr1[i];
  }
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add1"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("add2"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("id1"), 0UL);
  EXPECT_EQ(GET_UPDATE_CALL_TIMES("dsa1"), 0UL);
}

/**
 * todo 存在不支持刷新的model io时，这部分model io的更新函数不会被调用
 * todo 当前构造不出完全不需要刷新的task来
 */

TEST_F(ModelArgsManagerUT, UpdateForExecute_SqeLaunchedCorrect_SqePlacementExists) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL)
      .StubTaskInfo<DsaStubTaskInfo>(ModelTaskType::MODEL_TASK_DSA)
      .StubTaskInfo<RtsStubTaskInfo>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC);
  auto graph = gert::ShareGraph::BuildAiCoreRtsDsaToIdentityKnownShapeGraph();
  graph->TopologicalSorting();
  auto model_detail = gert::GeModelBuilder(graph)
                          .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                          .AddTaskDef("dsa1", gert::DsaCoreTaskDefFaker())
                          .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                          .AddTaskDef("id1", gert::RtsTaskDefFaker())
                          .BuildDetail();
  auto model = model_detail.model;
  auto node_names_to_task_indexes = ConvertToNoeKey(graph, model_detail.task_indexes_to_node_id);

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(false)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  for (size_t i = 0UL; i < task_list.size(); ++i) {
    mam.OnTaskDistributed(i, task_list[i].get());
  }

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ASSERT_EQ(runtime_stub.GetRtsRuntimeStub().GetLaunchSqeUpdateTaskArgs().size(), 1);

  auto &arg = *(runtime_stub.GetRtsRuntimeStub().GetLaunchSqeUpdateTaskArgs().begin());
  auto task_info = GET_TASK_INFO("dsa1");

  EXPECT_EQ(arg.stream_id, task_info->GetStreamId());
  EXPECT_EQ(arg.task_id, task_info->GetTaskID());
  EXPECT_EQ(
      arg.src,
      ValueToPtr(
          task_info->GetInitCalls().at(0).args.at(static_cast<int32_t>(ArgsPlacement::kArgsPlacementSqe)).dev_addr));
  EXPECT_EQ(arg.cnt, task_info->GetGenTaskArgsLen());
  EXPECT_EQ(arg.stm, (rtStream_t)1);
}
TEST_F(ModelArgsManagerUT, Compatibility_PrintWarningMessage_WhenHistoryTaskInfoExists) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetSlogStub().SetLevel(DLOG_INFO);
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());

  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  runtime_stub.GetSlogStub().FindWarnLogEndsWith(
      "CompatibleAllocation:There are still normal args allocation, not support in new version, known args len hbm/ts "
      "104/0");
  runtime_stub.GetSlogStub().FindInfoLogEndsWith("Begin to allocate fixed addr.");
  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 0;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);
}

TEST_F(ModelArgsManagerUT, GetRtsMemoryType_HostSvm) {
  ASSERT_EQ(GetRtsMemoryType(ArgsPlacement::kArgsPlacementHostSvm, 1), RT_MEMORY_HOST_SVM);
  ASSERT_EQ(GetRtsMemoryType(ArgsPlacement::kArgsPlacementHbm, 1), RT_MEMORY_HBM);
}

TEST_F(ModelArgsManagerUT, GenModelArgsRefreshInfosForTask) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());

  TaskArgsRefreshInfo info_high_32_bit = {
      0,
      0x32,
      0,
      0x64,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrHigh32Bit
  };

  TaskArgsRefreshInfo info_low_32_bit = {
      0,
      0x32,
      0,
      0x68,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrLow32Bit
  };
  std::vector<TaskArgsRefreshInfo> infos;
  infos.emplace_back(std::move(info_high_32_bit));
  infos.emplace_back(std::move(info_low_32_bit));
  mam.allocation_ids_to_model_args_refresh_infos_addr_high_32bit.resize(infos.size());
  mam.allocation_ids_to_model_args_refresh_infos_addr_low_32bit.resize(infos.size());
  PisToArgs args;
  args[0].dev_addr = PtrToValue(malloc(1024));
  void *host = nullptr;
  host = malloc(1024);
  args[0U].host_addr = host;
  args[0].len = 1024;

  auto node = BuildTestNode();
  ASSERT_EQ(mam.GenModelArgsRefreshInfosForTask(infos, args, node), SUCCESS);

  free(ValueToPtr(args[0].dev_addr));
  free(host);
}

TEST_F(ModelArgsManagerUT, GenAddrRefreshOpKernelLaunchArgsInfo_Test) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  //davinci_model.init();
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  dlog_setlevel(0,0,0);
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  TaskArgsRefreshInfo info_high_32_bit = {
      0,
      0,
      0,
      0,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrHigh32Bit
  };

  TaskArgsRefreshInfo info_low_32_bit = {
      0,
      0x20,
      0,
      0x20,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrLow32Bit
  };

  TaskArgsRefreshInfo info_all = {
      1,
      0x8,
      1,
      0x8,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrAll
  };

  std::vector<TaskArgsRefreshInfo> infos;
  infos.emplace_back(std::move(info_high_32_bit));
  infos.emplace_back(std::move(info_low_32_bit));
  infos.emplace_back(std::move(info_all));
  mam.allocation_ids_to_model_args_refresh_infos_addr_all.resize(infos.size());
  mam.allocation_ids_to_model_args_refresh_infos_addr_high_32bit.resize(infos.size());
  mam.allocation_ids_to_model_args_refresh_infos_addr_low_32bit.resize(infos.size());
  PisToArgs args;
  args[0].dev_addr = mam.model_args_[0].model_args_device_addr;
  args[0U].host_addr = mam.model_args_[0].model_args_host_addr.get();
  args[0].len = 1024;

  auto node = BuildTestNode();
  uint64_t offset_num = 5;
  ASSERT_EQ(mam.GenModelArgsRefreshInfosForTask(infos, args, node), SUCCESS);
  mam.model_args_len_[0] = 40;
  //ASSERT_EQ(mam.GenAddrRefreshOpKernelLaunchArgsInfo(pls), SUCCESS);
  mam.davinci_model_->logical_mem_allocations_.resize(40);
  ASSERT_EQ(mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->logical_mem_allocations_.size()), SUCCESS);
  ASSERT_EQ(mam.GenKernelLaunchArgs(offset_num), SUCCESS);
  ASSERT_EQ(mam.GenAddrRefreshIndexAndOffset(offset_num), SUCCESS);
  mam.launched_args_unique_ptr_ = nullptr;
  dlog_setlevel(0,3,0);
}

TEST_F(ModelArgsManagerUT, GenKernelLaunchArgs_with_300_modelio_Test) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();
  //davinci_model.init();
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  TaskArgsRefreshInfo info_high_32_bit = {
      0,
      0,
      0,
      0,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrHigh32Bit
  };

  TaskArgsRefreshInfo info_low_32_bit = {
      0,
      0x4,
      0,
      0x4,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrLow32Bit
  };

  TaskArgsRefreshInfo info_all = {
      1,
      0x8,
      1,
      0x8,
      ArgsPlacement::kArgsPlacementHbm,
      ArgsFormatPolicy::kAddrAll
  };

  std::vector<TaskArgsRefreshInfo> infos;
  infos.emplace_back(std::move(info_high_32_bit));
  infos.emplace_back(std::move(info_low_32_bit));
  infos.emplace_back(std::move(info_all));
  mam.allocation_ids_to_model_args_refresh_infos_addr_all.resize(infos.size());
  mam.allocation_ids_to_model_args_refresh_infos_addr_high_32bit.resize(infos.size());
  mam.allocation_ids_to_model_args_refresh_infos_addr_low_32bit.resize(infos.size());
  PisToArgs args;
  args[0].dev_addr = mam.model_args_[0].model_args_device_addr;
  args[0U].host_addr = mam.model_args_[0].model_args_host_addr.get();
  args[0].len = 1024;

  auto node = BuildTestNode();
  uint64_t offset_num = 300;
  mam.davinci_model_->logical_mem_allocations_.resize(300);
  //ArgsPlacement pls = ArgsPlacement::kArgsPlacementHbm;
  ASSERT_EQ(mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->logical_mem_allocations_.size()), SUCCESS);
  ASSERT_EQ(mam.GenKernelLaunchArgs(offset_num), SUCCESS);
}

TEST_F(ModelArgsManagerUT, GenAllocationToIowPaRemapInfos_TaskNoSupportPaRemap) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);

  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_.size(), 5UL);
  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_[0].size(), 3UL); // fm
  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_[1].size(), 2UL); // input1
  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_[2].size(), 1UL); // input2
  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_[3].size(), 1UL); // output
  ASSERT_EQ(mam.allocation_ids_to_iow_pa_remap_infos_[4].size(), 0UL); // absolute

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = 3;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  uint64_t va = mam.last_bases_[0]; //fm allocation
  uint64_t va_len = mam.id_to_len_[0];
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(va, 0, va_len, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 3);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, mam.last_bases_[0]);
  ASSERT_EQ(overlap_range[0].second, mam.last_bases_[0] + mam.id_to_len_[0] - 1);
}

TEST_F(ModelArgsManagerUT, PaRemapped_NoVaCrossOver) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [200，300）
  mam.last_bases_.emplace_back(200UL);
  mam.id_to_len_.emplace_back(100UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [210, 220)
  struct IowPaRemapInfo iow_pa_remap_info = {nullptr, 0U, 10UL, 10UL, PaRemapPolicy::KNoSupport};
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert(std::move(iow_pa_remap_info));

  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [100, 200)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(100, 0, 100, overlap_range), PARAM_INVALID);
  ASSERT_EQ(overlap_range.size(), 0);
};

TEST_F(ModelArgsManagerUT, PaRemapped_VaCrossOverWithNoTensor) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [200，300）
  mam.last_bases_.emplace_back(200UL);
  mam.id_to_len_.emplace_back(100UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [200, 300)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(200, 0, 100, overlap_range), SUCCESS);
  ASSERT_EQ(mam.pa_remap_match_support_num_, 1);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, 200);
  ASSERT_EQ(overlap_range[0].second, 299);
};


//|-------va-----------|
//    |------fm------------------|
//    |tensor1|
//       |tensor2|
//    |tensor3---|
//              |tensor4--|
TEST_F(ModelArgsManagerUT, PaRemapped_VaRightCrossOver) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [200，300）
  mam.last_bases_.emplace_back(200UL);
  mam.id_to_len_.emplace_back(100UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [200, 220)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 0UL, 20UL, PaRemapPolicy::KNoSupport});
  // tensor [210, 230)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 10UL, 20UL, PaRemapPolicy::KNoSupport});
  // tensor [200, 230)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 0UL, 30UL, PaRemapPolicy::KNoSupport});
  // tensor [230, 260)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 30UL, 30UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [100, 250)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(100, 0, 150, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 4);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, 200);
  ASSERT_EQ(overlap_range[0].second, 249);
};


//    |----------va-----------|
//|--------------fm--------------------|
//  |tensor1|  |tensor2|  |tensor3|
TEST_F(ModelArgsManagerUT, PaRemapped_VaAllCrossOver) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [150, 180)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 50UL, 30UL, PaRemapPolicy::KNoSupport});
  // tensor [190, 210)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 90UL, 20UL, PaRemapPolicy::KNoSupport});
  // tensor [220, 260)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 120UL, 40UL, PaRemapPolicy::KNoSupport});
  // tensor [250, 290)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 250UL, 40UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }

  // va [160, 250)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(160, 0, 90, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 3);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, 160);
  ASSERT_EQ(overlap_range[0].second, 249);
};


//           |---va----------|
//|---fm-----------------|
//   |tensor1| |tenosr2|
TEST_F(ModelArgsManagerUT, PaRemapped_VaLeftCrossOverWithHalfOpen) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [150, 180)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 50UL, 30UL, PaRemapPolicy::KNoSupport});
  // tensor [190, 210)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 90UL, 20UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [180, 320)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(180, 0, 140, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 1);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, 180);
  ASSERT_EQ(overlap_range[0].second, 299);
};


//           |---va----------|
//|---fm-----------------|
//     |tensor1| |tenosr2|
TEST_F(ModelArgsManagerUT, PaRemapped_VaLeftCrossOver) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [150, 180)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 50UL, 30UL, PaRemapPolicy::KNoSupport});
  // tensor [190, 210)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 90UL, 20UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [170, 320)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(170, 0, 150, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 2);
  ASSERT_EQ(overlap_range.size(), 1);
  ASSERT_EQ(overlap_range[0].first, 170);
  ASSERT_EQ(overlap_range[0].second, 299);
};


//      |----------va------------------|
//|-----fm1--------|    |---fm2-----------|
//   |tensor1|            |tensor2|
TEST_F(ModelArgsManagerUT, PaRemapped_VaCrossOverWithMultiFm) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(3);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // fm[1] [400，600）
  mam.last_bases_.emplace_back(400UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [150, 180)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 50UL, 30UL, PaRemapPolicy::KNoSupport});
  // tensor [420, 500)
  mam.allocation_ids_to_iow_pa_remap_infos_[1].insert({nullptr, 0U, 20UL, 80UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [170, 500)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(170, 0, 330, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 2);
  ASSERT_EQ(overlap_range.size(), 2);
  ASSERT_EQ(overlap_range[0].first, 170);
  ASSERT_EQ(overlap_range[0].second, 299);
  ASSERT_EQ(overlap_range[1].first, 400);
  ASSERT_EQ(overlap_range[1].second, 499);
};

//      |----------va------------------|
//|-----absolute----------------------------|
//          |tensor1|
TEST_F(ModelArgsManagerUT, PaRemapped_VaCrossOverWithAbsolute) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [400, 450)
  mam.allocation_ids_to_iow_pa_remap_infos_[1].insert({nullptr, 1U, 400UL, 50UL, PaRemapPolicy::KNoSupport});

  // tensor [500, 550)
  mam.allocation_ids_to_iow_pa_remap_infos_[1].insert({nullptr, 1U, 500UL, 50UL, PaRemapPolicy::KNoSupport});
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  // va [350, 600)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  ASSERT_EQ(mam.PaRemapped(350, 0, 300, overlap_range), FAILED);
  ASSERT_EQ(mam.pa_remap_match_nosupport_num_, 2);
  ASSERT_EQ(overlap_range.size(), 2);
  ASSERT_EQ(overlap_range[0].first, 400);
  ASSERT_EQ(overlap_range[0].second, 449);
  ASSERT_EQ(overlap_range[1].first, 500);
  ASSERT_EQ(overlap_range[1].second, 549);
};

//    |----------va-----------|
//|--------------fm--------------------|
//        |tensor1 empty|
TEST_F(ModelArgsManagerUT, PaRemapped_VaAllCrossOverWithEmptyTensor) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  ASSERT_EQ(mam.GetFixedAddrBulk().device_addr, nullptr);
  ASSERT_EQ(task_list.size(), 2UL);
  mam.allocation_ids_to_iow_pa_remap_infos_.clear();
  mam.allocation_ids_to_iow_pa_remap_infos_.resize(2);
  mam.last_bases_.clear();
  mam.id_to_len_.clear();

  // fm[0] [100，300）
  mam.last_bases_.emplace_back(100UL);
  mam.id_to_len_.emplace_back(200UL);

  // absolute
  mam.last_bases_.emplace_back(0UL);
  mam.id_to_len_.emplace_back(0xFFFFFFFFFFFFFFFFUL);

  // tensor [160, 160)
  mam.allocation_ids_to_iow_pa_remap_infos_[0].insert({nullptr, 0U, 160UL, 0UL, PaRemapPolicy::KNoSupport});

  // va [160, 250)
  std::vector<std::pair<uint64_t, uint64_t>> overlap_range;
  mam.AllocKernelLaunchArgsHostMem(mam.davinci_model_->GetLogicalMemAllocation().size());
  auto active_mem_base_ptr = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < mam.last_bases_.size(); i++) {
    active_mem_base_ptr[i] =  mam.last_bases_[i];
  }
  ASSERT_EQ(mam.PaRemapped(160, 0, 90, overlap_range), SUCCESS);
  ASSERT_EQ(overlap_range.size(), 1);
};

TEST_F(ModelArgsManagerUT, CalculateUpdateModelParamTiling_test) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  ModelArgsManager mam(davinci_model.get());
  UpdateModelParamTilingData tiling{0};
  uint32_t active_base_len = 512 * 8;
  uint32_t index_len = 32 * 1024;
  uint32_t block_dim{0};
  mam.CalculateUpdateModelParamTiling(active_base_len, index_len, block_dim, tiling);
  EXPECT_EQ(tiling.totalActiveBaseTblCnt, 1024);
  EXPECT_EQ(tiling.blockCnt, 2048);
  EXPECT_EQ(tiling.tileCnt, 2048);
  EXPECT_EQ(tiling.tailCnt, 2048);
  EXPECT_EQ(tiling.lastTailCnt, 2048);
  EXPECT_EQ(tiling.tileNum, 1);
  EXPECT_EQ(tiling.lastTileNum, 1);
  EXPECT_EQ(block_dim, 4);
  index_len = 128 * 1024;
  mam.CalculateUpdateModelParamTiling(active_base_len, index_len, block_dim, tiling);
  EXPECT_EQ(tiling.totalActiveBaseTblCnt, 1024);
  EXPECT_EQ(tiling.blockCnt, 2368);
  EXPECT_EQ(tiling.tileCnt, 2368);
  EXPECT_EQ(tiling.tailCnt, 2368);
  EXPECT_EQ(tiling.lastTailCnt, 1984);
  EXPECT_EQ(tiling.tileNum, 1);
  EXPECT_EQ(tiling.lastTileNum, 1);
  EXPECT_EQ(block_dim, 14);
};

TEST_F(ModelArgsManagerUT, UpdateForExecute_AclrtMemcpyAsync_BulkIncrement) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                            .SetFmRefreshable(true)
                            .GeModel(model)
                            .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                            .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtMemcpyAsync(void *dst, size_t dest_max, const void *src, size_t count,
                              aclrtMemcpyKind kind, void *stream) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
    aclError aclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, aclrtMemcpyKind kind) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
  };
  auto mock_acl_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_acl_runtime);

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ge::AclRuntimeStub::Reset();
};

TEST_F(ModelArgsManagerUT, UpdateForExecute_AclrtMemcpyAsync_HostInputOnly) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                            .SetFmRefreshable(true)
                            .GeModel(model)
                            .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                            .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  mam.host_input_size_ = 1024;

  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtMemcpyAsync(void *dst, size_t dest_max, const void *src, size_t count,
                              aclrtMemcpyKind kind, void *stream) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
  };
  auto mock_acl_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_acl_runtime);

  uint32_t up = 3;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ge::AclRuntimeStub::Reset();
};

TEST_F(ModelArgsManagerUT, UpdateForExecute_AclrtMemcpyAsync_PartialUpdate) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                            .SetFmRefreshable(true)
                            .GeModel(model)
                            .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                            .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtMemcpyAsync(void *dst, size_t dest_max, const void *src, size_t count,
                              aclrtMemcpyKind kind, void *stream) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
    aclError aclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, aclrtMemcpyKind kind) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
  };
  auto mock_acl_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_acl_runtime);

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ge::AclRuntimeStub::Reset();
};

TEST_F(ModelArgsManagerUT, UpdateForExecute_AclrtMemcpy_SyncMode) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                            .SetFmRefreshable(true)
                            .GeModel(model)
                            .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                            .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  rtStream_t stream = (rtStream_t)1;
  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  davinci_model->SetAsyncMode(false);

  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, aclrtMemcpyKind kind) override {
      if (dst != nullptr && src != nullptr && count > 0) {
        memcpy_s(dst, dest_max, src, count);
      }
      return ACL_SUCCESS;
    }
  };
  auto mock_acl_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_acl_runtime);

  uint32_t up = 4;
  ASSERT_EQ(mam.UpdateForExecute(up, stream), SUCCESS);

  ge::AclRuntimeStub::Reset();
};

// =====================================================================
// AllocateArgsBuffer Tier 1 (reserved segment) tests
// =====================================================================

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_FromReservedSegment_Success) {
  // Set up model_args_ with HBM placement
  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xABCD0000ULL;
  ModelArgsManager mam(nullptr);
  mam.model_args_.push_back(std::move(model_arg));

  // Set up reserved segment: 128 bytes capacity starting at offset 64
  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 64UL;
  mam.reserved_segments_[hbm_idx].current_offset = 64UL;
  mam.reserved_segments_[hbm_idx].total_size = 128UL;

  ArgsAllocationResult result;
  ASSERT_EQ(mam.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);

  EXPECT_TRUE(result.is_from_reserved);
  EXPECT_EQ(result.host_addr, mam.model_args_[0].model_args_host_addr.get() + 64UL);
  EXPECT_EQ(result.device_addr, 0xABCD0000ULL + 64UL);
  EXPECT_EQ(result.size, 32UL);
  EXPECT_EQ(result.placement, ArgsPlacement::kArgsPlacementHbm);
  EXPECT_EQ(result.extra_pool_index, std::numeric_limits<uint32_t>::max());

  // Verify offset advanced
  EXPECT_EQ(mam.reserved_segments_[hbm_idx].current_offset, 96UL);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_ReservedSegmentExhausted_FallsToExistingPool) {
  // Set up model_args_ with HBM placement
  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xABCD0000ULL;
  ModelArgsManager mam(nullptr);
  mam.model_args_.push_back(std::move(model_arg));

  // Exhausted reserved: only 8 bytes left (capacity=8, offset already at start+0)
  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 0UL;
  mam.reserved_segments_[hbm_idx].current_offset = 0UL;
  mam.reserved_segments_[hbm_idx].total_size = 8UL;

  // Set up extra pool as fallback (Tier 2)
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xDEADBEEFULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  ArgsAllocationResult result;
  // Requesting 32 bytes exceeds 8-byte reserved → falls to Tier 2 (existing pool)
  ASSERT_EQ(mam.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);

  EXPECT_FALSE(result.is_from_reserved);
  EXPECT_EQ(result.extra_pool_index, 0U);
  EXPECT_NE(result.host_addr, nullptr);
  EXPECT_EQ(result.size, 32UL);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_ReservedSegmentNoModelArg_FallsToExistingPool) {
  ModelArgsManager mam(nullptr);
  // No model_args_ at all — reserved segment exists but no ModelArg to reference

  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 0UL;
  mam.reserved_segments_[hbm_idx].current_offset = 0UL;
  mam.reserved_segments_[hbm_idx].total_size = 256UL;

  // Set up extra pool as fallback
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xF000ULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  ArgsAllocationResult result;
  ASSERT_EQ(mam.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);

  EXPECT_FALSE(result.is_from_reserved);
  EXPECT_EQ(result.extra_pool_index, 0U);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_MultipleReservedAllocations_OffsetAdvances) {
  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xBEEF0000ULL;
  ModelArgsManager mam(nullptr);
  mam.model_args_.push_back(std::move(model_arg));

  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 64UL;
  mam.reserved_segments_[hbm_idx].current_offset = 64UL;
  mam.reserved_segments_[hbm_idx].total_size = 80UL;  // Only 80 bytes: can hold 32+48=80 but not another 48

  ArgsAllocationResult result1, result2;
  ASSERT_EQ(mam.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result1), SUCCESS);
  EXPECT_TRUE(result1.is_from_reserved);
  EXPECT_EQ(result1.host_addr, mam.model_args_[0].model_args_host_addr.get() + 64UL);
  EXPECT_EQ(result1.device_addr, 0xBEEF0000ULL + 64UL);

  ASSERT_EQ(mam.AllocateArgsBuffer(48, ArgsPlacement::kArgsPlacementHbm, result2), SUCCESS);
  EXPECT_TRUE(result2.is_from_reserved);
  EXPECT_EQ(result2.host_addr, mam.model_args_[0].model_args_host_addr.get() + 96UL);
  EXPECT_EQ(result2.device_addr, 0xBEEF0000ULL + 96UL);

  // Third allocation (48 bytes) exceeds remaining reserved capacity → falls to extra pool
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xEEEEULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  ArgsAllocationResult result3;
  ASSERT_EQ(mam.AllocateArgsBuffer(48, ArgsPlacement::kArgsPlacementHbm, result3), SUCCESS);
  EXPECT_FALSE(result3.is_from_reserved);
}

// =====================================================================
// IntegrateCustomOpArgs pipeline tests
// =====================================================================

TEST_F(ModelArgsManagerUT, CollectTaskAllocationResults_ClassifiesReservedAndExtra) {
  ModelArgsManager mam(nullptr);

  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  // Task 0: 2 reserved results, 1 extra result
  ArgsAllocationResult reserved_result1;
  reserved_result1.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result1.device_addr = 0xA000ULL;
  reserved_result1.size = 32;
  reserved_result1.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result1.is_from_reserved = true;
  reserved_result1.extra_pool_index = std::numeric_limits<uint32_t>::max();

  ArgsAllocationResult reserved_result2;
  reserved_result2.host_addr = reinterpret_cast<void*>(0x1020ULL);
  reserved_result2.device_addr = 0xA020ULL;
  reserved_result2.size = 48;
  reserved_result2.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result2.is_from_reserved = true;
  reserved_result2.extra_pool_index = std::numeric_limits<uint32_t>::max();

  ArgsAllocationResult extra_result;
  extra_result.host_addr = reinterpret_cast<void*>(0x2000ULL);
  extra_result.device_addr = 0xD000ULL;
  extra_result.size = 64;
  extra_result.placement = ArgsPlacement::kArgsPlacementHbm;
  extra_result.is_from_reserved = false;
  extra_result.extra_pool_index = 0U;

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result1, reserved_result2, extra_result}));

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_reserved_results;
  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;

  ASSERT_EQ(mam.CollectTaskAllocationResults(task_reserved_results, task_extra_results), SUCCESS);

  // Task 0 has 2 reserved results
  ASSERT_EQ(task_reserved_results.size(), 1U);
  ASSERT_EQ(task_reserved_results[0].size(), 2U);
  EXPECT_TRUE(task_reserved_results[0][0].is_from_reserved);
  EXPECT_TRUE(task_reserved_results[0][1].is_from_reserved);

  // Task 0 has 1 extra result
  ASSERT_EQ(task_extra_results.size(), 1U);
  ASSERT_EQ(task_extra_results[0].size(), 1U);
  EXPECT_FALSE(task_extra_results[0][0].is_from_reserved);
  EXPECT_EQ(task_extra_results[0][0].extra_pool_index, 0U);
}

TEST_F(ModelArgsManagerUT, IntegrateReservedH2DCopyDatas_CreatesH2DForReservedSegment) {
  ModelArgsManager mam(nullptr);

  // Set up model_args_ with HBM placement (128 bytes total)
  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xBEEF0000ULL;
  mam.model_args_.push_back(std::move(model_arg));

  // Reserved segment: used 64 bytes (current_offset=128, start=64, total=128 → used=64)
  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 64UL;
  mam.reserved_segments_[hbm_idx].current_offset = 128UL;
  mam.reserved_segments_[hbm_idx].total_size = 128UL;

  // Initialize update_policies_to_model_data_ for ModelArgsManager::kUpdateModelIo (existing from built-in args)
  mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo] = ge::MakeUnique<ModelArgsManager::ArgsUpdateData>();
  ModelArgsManager::H2DCopyArg existing_h2d;
  existing_h2d.len = 64UL;
  existing_h2d.device_addr = 0xBEEF0000ULL;
  existing_h2d.host_addr = mam.model_args_[0].model_args_host_addr.get();
  existing_h2d.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo]->h2d_copy_datas.push_back(std::move(existing_h2d));

  ASSERT_EQ(mam.IntegrateReservedH2DCopyDatas(), SUCCESS);

  // Verify H2D copy expanded for ModelArgsManager::kUpdateModelIo HBM placement
  auto &model_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo];
  ASSERT_NE(model_data, nullptr);
  ASSERT_EQ(model_data->h2d_copy_datas.size(), 1U);
  // len should be expanded from 64 (built-in) + 64 (reserved used)
  EXPECT_EQ(model_data->h2d_copy_datas[0].len, 128UL);
}

TEST_F(ModelArgsManagerUT, IntegrateReservedH2DCopyDatas_NoReservedUsed_Skips) {
  ModelArgsManager mam(nullptr);

  // No reserved usage (current_offset == start_offset means 0 bytes used)
  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 0UL;
  mam.reserved_segments_[hbm_idx].current_offset = 0UL;
  mam.reserved_segments_[hbm_idx].total_size = 128UL;

  ASSERT_EQ(mam.IntegrateReservedH2DCopyDatas(), SUCCESS);

  // No update_policies_to_model_data_ created since nothing used
  for (size_t i = 0; i < ModelArgsManager::kUpdatePolicyEnd; ++i) {
    EXPECT_EQ(mam.update_policies_to_model_data_[i], nullptr);
  }
}

TEST_F(ModelArgsManagerUT, IntegrateExtraH2DCopyDatas_CreatesH2DForPools) {
  ModelArgsManager mam(nullptr);

  // Set up 2 extra pools with allocated content
  ModelArgsManager::ExtraArgsPool pool1;
  pool1.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool1.device_addr = 0xD000ULL;
  pool1.total_size = 4096UL;
  pool1.allocated_offset = 64UL;  // 64 bytes allocated
  pool1.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool1));

  ModelArgsManager::ExtraArgsPool pool2;
  pool2.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool2.device_addr = 0xE000ULL;
  pool2.total_size = 4096UL;
  pool2.allocated_offset = 128UL;  // 128 bytes allocated
  pool2.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool2));

  ASSERT_EQ(mam.IntegrateExtraH2DCopyDatas(), SUCCESS);

  // Verify extra_policy_to_update_datas_ has entries for ModelArgsManager::kUpdateModelIo
  auto &extra_datas = mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo];
  ASSERT_EQ(extra_datas.size(), 2U);

  EXPECT_EQ(extra_datas[0].h2d_copy_datas.size(), 1U);
  EXPECT_EQ(extra_datas[0].h2d_copy_datas[0].len, 64UL);
  EXPECT_EQ(extra_datas[0].h2d_copy_datas[0].device_addr, 0xD000ULL);

  EXPECT_EQ(extra_datas[1].h2d_copy_datas.size(), 1U);
  EXPECT_EQ(extra_datas[1].h2d_copy_datas[0].len, 128UL);
  EXPECT_EQ(extra_datas[1].h2d_copy_datas[0].device_addr, 0xE000ULL);
}

TEST_F(ModelArgsManagerUT, IntegrateExtraH2DCopyDatas_EmptyPool_Skips) {
  ModelArgsManager mam(nullptr);

  // Pool with 0 allocated offset → skipped
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xD000ULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;  // Nothing allocated
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  ASSERT_EQ(mam.IntegrateExtraH2DCopyDatas(), SUCCESS);

  auto &extra_datas = mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo];
  EXPECT_EQ(extra_datas.size(), 0U);
}

TEST_F(ModelArgsManagerUT, IntegrateReservedUpdateDatas_PopulatesUpdateHostArgsArg) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult reserved_result;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xA000ULL;
  reserved_result.size = 64;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.is_from_reserved = true;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result}));

  mam.custom_op_task_to_policies_[0] = {ModelArgsManager::kUpdateModelIo};

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_reserved_results;
  task_reserved_results[0] = {reserved_result};

  ASSERT_EQ(mam.IntegrateReservedUpdateDatas(task_reserved_results), SUCCESS);

  // Verify update_policies_to_model_data_ populated for ModelArgsManager::kUpdateModelIo
  auto &model_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo];
  ASSERT_NE(model_data, nullptr);
  ASSERT_EQ(model_data->update_datas.size(), 1U);
  EXPECT_EQ(model_data->update_datas[0].task_index, 0UL);
  ASSERT_NE(model_data->update_datas[0].task_info, nullptr);
  EXPECT_EQ(model_data->update_datas[0].host_args.size(), 1U);
  EXPECT_EQ(model_data->update_datas[0].host_args[0].addr, reinterpret_cast<void*>(0x1000ULL));
  EXPECT_EQ(model_data->update_datas[0].host_args[0].len, 64);
  EXPECT_EQ(model_data->update_datas[0].host_args[0].placement, ArgsPlacement::kArgsPlacementHbm);

  // Verify custom_op_policies_to_task_infos_ populated
  auto &policies_to_task_infos = mam.custom_op_policies_to_task_infos_;
  ASSERT_TRUE(policies_to_task_infos.find(ModelArgsManager::kUpdateModelIo) != policies_to_task_infos.end());
  EXPECT_EQ(policies_to_task_infos[ModelArgsManager::kUpdateModelIo].size(), 1U);
}

TEST_F(ModelArgsManagerUT, IntegrateReservedUpdateDatas_MultiplePolicies) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult reserved_result;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xA000ULL;
  reserved_result.size = 64;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.is_from_reserved = true;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result}));

  // Custom op needs both kUpdateFmAndModelIo and kUpdateModelIo
  mam.custom_op_task_to_policies_[0] = {ModelArgsManager::kUpdateFmAndModelIo,
                                         ModelArgsManager::kUpdateModelIo};

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_reserved_results;
  task_reserved_results[0] = {reserved_result};

  ASSERT_EQ(mam.IntegrateReservedUpdateDatas(task_reserved_results), SUCCESS);

  // Verify update_data populated for both policies
  auto &fm_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateFmAndModelIo];
  ASSERT_NE(fm_data, nullptr);
  EXPECT_EQ(fm_data->update_datas.size(), 1U);
  EXPECT_EQ(fm_data->update_datas[0].task_index, 0UL);

  auto &io_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo];
  ASSERT_NE(io_data, nullptr);
  EXPECT_EQ(io_data->update_datas.size(), 1U);
  EXPECT_EQ(io_data->update_datas[0].task_index, 0UL);

  // Verify custom_op_policies_to_task_infos_ populated for both policies
  auto &policies_to_task_infos = mam.custom_op_policies_to_task_infos_;
  EXPECT_EQ(policies_to_task_infos[ModelArgsManager::kUpdateFmAndModelIo].size(), 1U);
  EXPECT_EQ(policies_to_task_infos[ModelArgsManager::kUpdateModelIo].size(), 1U);
}

TEST_F(ModelArgsManagerUT, IntegrateReservedUpdateDatas_SkipsTaskNotInPolicies) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult reserved_result;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xA000ULL;
  reserved_result.size = 64;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.is_from_reserved = true;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result}));

  // No custom_op_task_to_policies_ entry — task should be skipped
  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_reserved_results;
  task_reserved_results[0] = {reserved_result};

  ASSERT_EQ(mam.IntegrateReservedUpdateDatas(task_reserved_results), SUCCESS);

  // Verify no update_data populated — task was skipped
  auto &model_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo];
  EXPECT_EQ(model_data, nullptr);
  EXPECT_TRUE(mam.custom_op_policies_to_task_infos_.empty());
}

TEST_F(ModelArgsManagerUT, IntegrateExtraUpdateDatas_PopulatesPoolUpdateDatas) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult extra_result;
  extra_result.host_addr = reinterpret_cast<void*>(0x2000ULL);
  extra_result.device_addr = 0xD000ULL;
  extra_result.size = 32;
  extra_result.placement = ArgsPlacement::kArgsPlacementHbm;
  extra_result.is_from_reserved = false;
  extra_result.extra_pool_index = 0U;

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{extra_result}));

  mam.custom_op_task_to_policies_[0] = {ModelArgsManager::kUpdateModelIo};

  // Set up extra pool
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xD000ULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 32UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;
  task_extra_results[0] = {extra_result};

  ASSERT_EQ(mam.IntegrateExtraUpdateDatas(task_extra_results), SUCCESS);

  auto &extra_datas = mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo];
  ASSERT_EQ(extra_datas.size(), 1U);  // resized to match pools count
  ASSERT_EQ(extra_datas[0].update_datas.size(), 1U);
  EXPECT_EQ(extra_datas[0].update_datas[0].task_index, 0UL);
  EXPECT_EQ(extra_datas[0].update_datas[0].host_args.size(), 1U);
  EXPECT_EQ(extra_datas[0].update_datas[0].host_args[0].addr, reinterpret_cast<void*>(0x2000ULL));
  EXPECT_EQ(extra_datas[0].update_datas[0].host_args[0].len, 32);
}

TEST_F(ModelArgsManagerUT, IntegrateCustomOpArgs_FullPipeline_ReservedAndExtra) {
  ModelArgsManager mam(nullptr);
  mam.has_reserve_args_table_ = true;
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  // Task 0: 1 reserved + 1 extra
  ArgsAllocationResult reserved_result;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xBEEF0040ULL;
  reserved_result.size = 32;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.is_from_reserved = true;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  ArgsAllocationResult extra_result;
  extra_result.host_addr = reinterpret_cast<void*>(0x2000ULL);
  extra_result.device_addr = 0xD000ULL;
  extra_result.size = 64;
  extra_result.placement = ArgsPlacement::kArgsPlacementHbm;
  extra_result.is_from_reserved = false;
  extra_result.extra_pool_index = 0U;

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result, extra_result}));

  // Set up model_args_ with HBM placement
  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xBEEF0000ULL;
  mam.model_args_.push_back(std::move(model_arg));

  // Reserved segment: 32 bytes used
  const size_t hbm_idx = static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm);
  mam.reserved_segments_[hbm_idx].start_offset = 64UL;
  mam.reserved_segments_[hbm_idx].current_offset = 96UL;  // 32 bytes used
  mam.reserved_segments_[hbm_idx].total_size = 128UL;

  // Extra pool
  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xD000ULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 64UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  mam.custom_op_task_to_policies_[0] = {ModelArgsManager::kUpdateModelIo};

  // Existing built-in update data for ModelArgsManager::kUpdateModelIo
  mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo] = ge::MakeUnique<ModelArgsManager::ArgsUpdateData>();
  ModelArgsManager::H2DCopyArg existing_h2d;
  existing_h2d.len = 64UL;
  existing_h2d.device_addr = 0xBEEF0000ULL;
  existing_h2d.host_addr = mam.model_args_[0].model_args_host_addr.get();
  existing_h2d.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo]->h2d_copy_datas.push_back(std::move(existing_h2d));

  ASSERT_EQ(mam.IntegrateCustomOpArgs(), SUCCESS);

  // Verify reserved H2D expanded
  auto &model_data = mam.update_policies_to_model_data_[ModelArgsManager::kUpdateModelIo];
  ASSERT_NE(model_data, nullptr);
  ASSERT_EQ(model_data->h2d_copy_datas.size(), 1U);
  EXPECT_EQ(model_data->h2d_copy_datas[0].len, 96UL);  // 64 (built-in) + 32 (reserved used)

  // Verify reserved update data populated
  ASSERT_EQ(model_data->update_datas.size(), 1U);
  EXPECT_EQ(model_data->update_datas[0].task_index, 0UL);

  // Verify extra H2D created
  auto &extra_datas = mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo];
  ASSERT_EQ(extra_datas.size(), 1U);
  ASSERT_EQ(extra_datas[0].h2d_copy_datas.size(), 1U);
  EXPECT_EQ(extra_datas[0].h2d_copy_datas[0].len, 64UL);

  // Verify extra update data populated
  ASSERT_EQ(extra_datas[0].update_datas.size(), 1U);
  EXPECT_EQ(extra_datas[0].update_datas[0].task_index, 0UL);
  EXPECT_EQ(extra_datas[0].update_datas[0].host_args.size(), 1U);
}

// =====================================================================
// Task 4: IntegrateCustomOpArgs error path UTs
// =====================================================================

TEST_F(ModelArgsManagerUT, IntegrateCustomOpArgs_NullTaskListPtr_ReturnsSuccess) {
  ModelArgsManager mam(nullptr);
  mam.has_reserve_args_table_ = true;
  // task_list_ptr_ is nullptr → IntegrateCustomOpArgs returns SUCCESS (early exit)
  ASSERT_EQ(mam.IntegrateCustomOpArgs(), SUCCESS);
}

TEST_F(ModelArgsManagerUT, IntegrateCustomOpArgs_NoReservedArgsTable_ReturnsSuccess) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;
  mam.has_reserve_args_table_ = false;
  ASSERT_EQ(mam.IntegrateCustomOpArgs(), SUCCESS);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_FromExistingPool_Success) {
  DavinciModel model(0, nullptr);
  InsertStubAllocator(&model);
  ModelArgsManager mam(&model);
  mam.davinci_model_ = &model;

  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xD000ULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  mam.extra_args_pools_.emplace_back(std::move(pool));

  ArgsAllocationResult result;
  ASSERT_EQ(mam.AllocateArgsBuffer(64, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);
  EXPECT_EQ(result.is_from_reserved, false);
  EXPECT_EQ(result.extra_pool_index, 0U);
  EXPECT_EQ(result.size, 64UL);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_FromNewPool_Success) {
  DavinciModel model(0, nullptr);
  InsertStubAllocator(&model);
  ModelArgsManager mam(&model);
  mam.davinci_model_ = &model;

  ArgsAllocationResult result;
  ASSERT_EQ(mam.AllocateArgsBuffer(64, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);
  EXPECT_EQ(result.is_from_reserved, false);
  EXPECT_EQ(mam.extra_args_pools_.size(), 1U);
  EXPECT_GE(mam.extra_args_pools_[0].total_size, 4096UL);
}

TEST_F(ModelArgsManagerUT, AllocateArgsBuffer_ZeroSize_AssertFails) {
  DavinciModel model(0, nullptr);
  InsertStubAllocator(&model);
  ModelArgsManager mam(&model);
  mam.davinci_model_ = &model;

  ArgsAllocationResult result;
  EXPECT_NE(mam.AllocateArgsBuffer(0, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);
}

TEST_F(ModelArgsManagerUT, CollectTaskAllocationResults_SeparatesReservedAndExtra) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult reserved_result;
  reserved_result.is_from_reserved = true;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xA000ULL;
  reserved_result.size = 32;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  ArgsAllocationResult extra_result;
  extra_result.is_from_reserved = false;
  extra_result.host_addr = reinterpret_cast<void*>(0x2000ULL);
  extra_result.device_addr = 0xD000ULL;
  extra_result.size = 64;
  extra_result.placement = ArgsPlacement::kArgsPlacementHbm;
  extra_result.extra_pool_index = 0U;

  mam.task_list_ptr_->push_back(
      std::make_shared<MockTaskInfoWithAllocResults>(
          std::vector<ArgsAllocationResult>{reserved_result, extra_result}));

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_reserved_results;
  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;

  ASSERT_EQ(mam.CollectTaskAllocationResults(task_reserved_results, task_extra_results), SUCCESS);
  EXPECT_EQ(task_reserved_results.size(), 1U);
  EXPECT_EQ(task_reserved_results[0].size(), 1U);
  EXPECT_EQ(task_reserved_results[0][0].is_from_reserved, true);
  EXPECT_EQ(task_extra_results.size(), 1U);
  EXPECT_EQ(task_extra_results[0].size(), 1U);
  EXPECT_EQ(task_extra_results[0][0].is_from_reserved, false);
}

TEST_F(ModelArgsManagerUT, FindOrCreateUpdateArg_CreatesNewAndFindsExisting) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;
  auto task_info = std::make_shared<MockTaskInfoWithAllocResults>(std::vector<ArgsAllocationResult>{});
  mam.task_list_ptr_->push_back(task_info);

  ModelArgsManager::ArgsUpdateData update_data;

  auto* arg1 = mam.FindOrCreateUpdateArg(update_data, 0UL, task_info.get());
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->task_index, 0UL);
  EXPECT_EQ(update_data.update_datas.size(), 1U);

  auto* arg2 = mam.FindOrCreateUpdateArg(update_data, 0UL, task_info.get());
  EXPECT_EQ(arg2, arg1);
  EXPECT_EQ(update_data.update_datas.size(), 1U);

  auto* arg3 = mam.FindOrCreateUpdateArg(update_data, 1UL, nullptr);
  ASSERT_NE(arg3, nullptr);
  EXPECT_EQ(arg3->task_index, 1UL);
  EXPECT_EQ(update_data.update_datas.size(), 2U);
}

TEST_F(ModelArgsManagerUT, AppendHostArgs_AppendsCorrectly) {
  ModelArgsManager mam(nullptr);
  ModelArgsManager::UpdateHostArgsArg update_arg;

  ArgsAllocationResult result1;
  result1.host_addr = reinterpret_cast<void*>(0x1000ULL);
  result1.size = 32;
  result1.placement = ArgsPlacement::kArgsPlacementHbm;

  ArgsAllocationResult result2;
  result2.host_addr = reinterpret_cast<void*>(0x2000ULL);
  result2.size = 64;
  result2.placement = ArgsPlacement::kArgsPlacementHbm;

mam.AppendHostArgs(&update_arg, {result1, result2});
  EXPECT_EQ(update_arg.host_args.size(), 2);
}

TEST_F(ModelArgsManagerUT, IntegrateExtraUpdateDatas_SkipsUnknownTaskIndex) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;
  auto mock_task = std::make_shared<MockTaskInfoWithAllocResults>(std::vector<ArgsAllocationResult>{});
  task_list.push_back(mock_task);

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;
  ArgsAllocationResult result;
  result.is_from_reserved = false;
  result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  result.device_addr = 0xD000ULL;
  result.size = 64;
  result.placement = ArgsPlacement::kArgsPlacementHbm;
  result.extra_pool_index = 0U;
  task_extra_results[0UL] = {result};

  mam.custom_op_task_to_policies_[0UL] = {ModelArgsManager::kUpdateModelIo};
  mam.extra_args_pools_.push_back({ge::MakeUnique<uint8_t[]>(256), 0xA000ULL, 256UL, 0UL, ArgsPlacement::kArgsPlacementHbm});

ASSERT_EQ(mam.IntegrateExtraUpdateDatas(task_extra_results), SUCCESS);
  EXPECT_EQ(mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo].size(), 1U);
  EXPECT_EQ(mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo].size(), 1U);
}

TEST_F(ModelArgsManagerUT, Init_NeedReserveArgsTable_SetsFlagsAndReservedSegments) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub()
      .StubTaskInfo<CustomReserveArgsStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);

  EXPECT_TRUE(mam.has_reserve_args_table_);
  EXPECT_GT(mam.reserved_segments_[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].total_size, 0U);
}

TEST_F(ModelArgsManagerUT, IntegrateExtraUpdateDatas_SkipsOutOfPoolRange) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;
  auto mock_task = std::make_shared<MockTaskInfoWithAllocResults>(std::vector<ArgsAllocationResult>{});
  task_list.push_back(mock_task);

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;
  ArgsAllocationResult result;
  result.is_from_reserved = false;
  result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  result.device_addr = 0xD000ULL;
  result.size = 64;
  result.placement = ArgsPlacement::kArgsPlacementHbm;
  result.extra_pool_index = 100U;
  task_extra_results[0UL] = {result};

  mam.custom_op_task_to_policies_[0UL] = {ModelArgsManager::kUpdateModelIo};
  mam.extra_args_pools_.push_back({ge::MakeUnique<uint8_t[]>(256), 0xA000ULL, 256UL, 0UL, ArgsPlacement::kArgsPlacementHbm});

  ASSERT_EQ(mam.IntegrateExtraUpdateDatas(task_extra_results), SUCCESS);
  auto &extra_datas = mam.extra_policy_to_update_datas_[ModelArgsManager::kUpdateModelIo];
  EXPECT_EQ(extra_datas.size(), 1U);
  EXPECT_EQ(extra_datas[0].update_datas.size(), 0U);
}

TEST_F(ModelArgsManagerUT, IntegrateExtraUpdateDatas_SkipsTaskWithoutPolicy) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;
  auto mock_task = std::make_shared<MockTaskInfoWithAllocResults>(std::vector<ArgsAllocationResult>{});
  task_list.push_back(mock_task);

  std::unordered_map<size_t, std::vector<ArgsAllocationResult>> task_extra_results;
  ArgsAllocationResult result;
  result.is_from_reserved = false;
  result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  result.device_addr = 0xD000ULL;
  result.size = 64;
  result.placement = ArgsPlacement::kArgsPlacementHbm;
  result.extra_pool_index = 0U;
  task_extra_results[0UL] = {result};

  ASSERT_EQ(mam.IntegrateExtraUpdateDatas(task_extra_results), SUCCESS);
  EXPECT_EQ(mam.extra_policy_to_update_datas_.size(), 0U);
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_CustomOpPoliciesTriggersUpdateHostArgs) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 1U);

  auto mock_custom_op = std::make_shared<MockCustomOpTaskInfo>();
  mam.has_reserve_args_table_ = true;
  mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo].insert(mock_custom_op.get());

  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = ModelArgsManager::kUpdateModelIo;
  ASSERT_EQ(mam.UpdateForExecute(up, (rtStream_t)1), SUCCESS);
  EXPECT_EQ(mock_custom_op->update_host_args_void_calls_, 1);
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_CustomOpPoliciesEmptyDoesNotCrash) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 1U);

  mam.has_reserve_args_table_ = true;
  mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo];

  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = ModelArgsManager::kUpdateModelIo;
  ASSERT_EQ(mam.UpdateForExecute(up, (rtStream_t)1), SUCCESS);
}

TEST_F(ModelArgsManagerUT, IntegrateCustomOpArgs_TaskWithBothReservedAndExtra_NoDuplicateTaskInfo) {
  ModelArgsManager mam(nullptr);
  std::vector<TaskInfoPtr> task_list;
  mam.task_list_ptr_ = &task_list;

  ArgsAllocationResult reserved_result;
  reserved_result.is_from_reserved = true;
  reserved_result.host_addr = reinterpret_cast<void*>(0x1000ULL);
  reserved_result.device_addr = 0xA000ULL;
  reserved_result.size = 32;
  reserved_result.placement = ArgsPlacement::kArgsPlacementHbm;
  reserved_result.extra_pool_index = std::numeric_limits<uint32_t>::max();

  ArgsAllocationResult extra_result;
  extra_result.is_from_reserved = false;
  extra_result.host_addr = reinterpret_cast<void*>(0x2000ULL);
  extra_result.device_addr = 0xD000ULL;
  extra_result.size = 64;
  extra_result.placement = ArgsPlacement::kArgsPlacementHbm;
  extra_result.extra_pool_index = 0U;

  auto mock_task = std::make_shared<MockTaskInfoWithAllocResults>(
      std::vector<ArgsAllocationResult>{reserved_result, extra_result});
  task_list.push_back(mock_task);

  mam.custom_op_task_to_policies_[0UL] = {ModelArgsManager::kUpdateModelIo};

  ModelArgsManager::ModelArgs model_arg;
  model_arg.placement = ArgsPlacement::kArgsPlacementHbm;
  model_arg.model_args_host_addr = ge::MakeUnique<uint8_t[]>(256);
  model_arg.model_args_device_addr = 0xABCD0000ULL;
  mam.model_args_.push_back(std::move(model_arg));
  mam.reserved_segments_[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)] = {0UL, 128UL, 32UL};

  mam.extra_args_pools_.push_back(
      {ge::MakeUnique<uint8_t[]>(256), 0xE000ULL, 256UL, 64UL, ArgsPlacement::kArgsPlacementHbm});

  mam.has_reserve_args_table_ = true;
  ASSERT_EQ(mam.IntegrateCustomOpArgs(), SUCCESS);

  auto &task_infos = mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo];
  EXPECT_EQ(task_infos.size(), 1U);
}

TEST_F(ModelArgsManagerUT, UpdateForExecute_MultipleCustomOps_AllCalledOnce) {
  gert::GertRuntimeStub runtime_stub;
  runtime_stub.GetTaskInfoFactoryStub().StubTaskInfo<AicoreStubTaskInfo>(ModelTaskType::MODEL_TASK_KERNEL);
  auto graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  graph->TopologicalSorting();
  auto model = gert::GeModelBuilder(graph)
                   .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1"))
                   .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2"))
                   .Build();

  auto davinci_model = DavinciModelFaker()
                           .SetFmRefreshable(true)
                           .GeModel(model)
                           .GenerateSymbolForTaskInfoFaker(&(runtime_stub.GetTaskInfoFactoryStub()))
                           .Build();

  InsertStubAllocator(davinci_model.get());
  ModelArgsManager mam(davinci_model.get());
  std::vector<TaskInfoPtr> task_list;
  ASSERT_EQ(mam.Init(*(model->GetModelTaskDefPtr()), &task_list), SUCCESS);
  mam.SetAllocationHitCount(1U, 1U);

  auto mock_op1 = std::make_shared<MockCustomOpTaskInfo>();
  auto mock_op2 = std::make_shared<MockCustomOpTaskInfo>();
  auto mock_op3 = std::make_shared<MockCustomOpTaskInfo>();
  mam.has_reserve_args_table_ = true;
  mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo].insert(mock_op1.get());
  mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo].insert(mock_op2.get());
  mam.custom_op_policies_to_task_infos_[ModelArgsManager::kUpdateModelIo].insert(mock_op3.get());

  std::vector<uint64_t> active_mem_base_addr = ActiveMemBaseFaker(2, 1).FmBaseIndex(0).ModelIoBaseIndex(1).Build();
  mam.AllocKernelLaunchArgsHostMem(active_mem_base_addr.size());
  uint64_t* active_mem_base_addr_temp = mam.GetActivateMemBaseAddrs();
  for (size_t i = 0; i < active_mem_base_addr.size(); i++) {
    active_mem_base_addr_temp[i] = active_mem_base_addr[i];
  }

  uint32_t up = ModelArgsManager::kUpdateModelIo;
  ASSERT_EQ(mam.UpdateForExecute(up, (rtStream_t)1), SUCCESS);
  EXPECT_EQ(mock_op1->update_host_args_void_calls_, 1);
  EXPECT_EQ(mock_op2->update_host_args_void_calls_, 1);
  EXPECT_EQ(mock_op3->update_host_args_void_calls_, 1);
}

}  // namespace ge
