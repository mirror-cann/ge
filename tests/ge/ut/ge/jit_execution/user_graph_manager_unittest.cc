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
#include "ge_graph_dsl/graph_dsl.h"
#include "es_ge_test_ops.h"
#include "graph/utils/graph_utils_ex.h"
#include "jit_execution/user_graphs_manager.h"
#include "stub/gert_runtime_stub.h"
#include <vector>
#include "jit_share_graph.h"
#include "common/model/external_allocator_manager.h"
#include "ge/st/stubs/utils/mock_ops_kernel_builder.h"
#include "ge_running_env/dir_env.h"
#include "faker/space_registry_faker.h"
#include "common_setup.h"
#include "ge/ge_api.h"
#include "api/aclgrph/option_utils.h"
#include "common/memory/tensor_trans_utils.h"
#include "graph/execute/model_executor.h"
#include "graph_metadef/depends/checker/tensor_check_utils.h"
#include "common/platform_context.h"
using namespace testing;

namespace ge {
bool EnableSliceSchedule() {  // 桩函数
  return true;
}
class RuntimeMock : public RuntimeStub {
 public:
  rtError_t rtGetSocSpec(const char *label, const char *key, char *val, const uint32_t maxLen) override {
    (void)label;
    (void)key;
    (void)strcpy_s(val, maxLen, "fake");  // 用例不应该走自动融合
    return RT_ERROR_NONE;
  }
};

class UserGraphsManagerlUT : public testing::Test {
 protected:
  void SetUp() override {
    CommonSetupUtil::CommonSetup();
    gert_stub_.GetKernelStub().StubTiling();
    RuntimeStub::Install(nullptr);  // gert的rts stub不能在多线程环境下工作，因此使用默认rts stub
    AclRuntimeStub::Install(nullptr);
    RuntimeStub::SetInstance(std::make_shared<RuntimeMock>());
    gert::SpaceRegistryFaker::CreateDefaultSpaceRegistry();
    std::map<std::string, std::string> options = {{ge::SOC_VERSION, "Ascend310"}};
    GetThreadLocalContext().SetGlobalOption(options);
    ge::PlatformContext::GetInstance().SetPlatform("fake");
    const auto env_ptr = getenv("LD_PRELOAD");
    if (env_ptr != nullptr) {
      env = env_ptr;
      unsetenv("LD_PRELOAD");
    }
  }
  void TearDown() override {
    unsetenv("AUTOFUSE_FLAGS");
    CommonSetupUtil::CommonTearDown();
    gert_stub_.Clear();
    RuntimeStub::Reset();
    AclRuntimeStub::Reset();
    ge::PlatformContext::GetInstance().Reset();
    if (!env.empty()) {
      setenv("LD_PRELOAD", env.c_str(), 1);
    }
  }
  gert::GertRuntimeStub gert_stub_;
  std::string env;
};

TEST_F(UserGraphsManagerlUT, AddGraph_RemoveGraph_Success) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.BuildGraph(user_graph_id, {}, 0), SUCCESS);
  EXPECT_FALSE(user_graph_manager.IsGraphNeedRebuild(user_graph_id));
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}
TEST_F(UserGraphsManagerlUT, AddGraph_Twice_Success) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.BuildGraph(user_graph_id, {}, 0), SUCCESS);
  EXPECT_FALSE(user_graph_manager.IsGraphNeedRebuild(user_graph_id));
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, RemoveGraph_NotExist) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  EXPECT_EQ(user_graph_manager.RemoveGraph(0), FAILED);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, GetSetCompiledFlag_Failed) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  bool flag = false;
  EXPECT_NE(user_graph_manager.GetCompiledFlag(0, flag), SUCCESS);
  EXPECT_NE(user_graph_manager.SetCompiledFlag(0, flag), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, RunGraphAsync_Success) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  // prepare run task
  std::vector<int64_t> shape_dim = {2, 3, 3, 2};
  std::vector<gert::Tensor> inputs(1);
  TensorCheckUtils::ConstructGertTensor(inputs[0], {2, 3, 3, 2}, DT_FLOAT, FORMAT_NCHW);
  const RunAsyncCallbackV2 callback = [&](Status status, std::vector<gert::Tensor> &outputs) {
    EXPECT_EQ(status, SUCCESS);
    ASSERT_EQ(outputs.size(), 1);
    auto out_shape = outputs[0].GetStorageShape();
    auto out_dims = TensorTransUtils::GetDimsFromGertShape(out_shape);
    EXPECT_EQ(out_dims, shape_dim);
  };
  EXPECT_EQ(user_graph_manager.RunGraphAsync(user_graph_id, std::move(inputs), 0, callback), SUCCESS);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, IsGraphNeedRebuild_False) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options = {{ge::INPUT_HINT_SHAPE, "0:[2,3,4,5]"}};
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  // prepare run task
  std::vector<int64_t> shape_dim = {2, 3, 3, 2};
  std::vector<gert::Tensor> inputs(1);
  TensorCheckUtils::ConstructGertTensor(inputs[0], {2, 3, 3, 2}, DT_FLOAT, FORMAT_NCHW);
  const RunAsyncCallbackV2 callback = [&](Status status, std::vector<gert::Tensor> &outputs) {
    EXPECT_EQ(status, SUCCESS);
    EXPECT_EQ(outputs.size(), 1);
    auto out_shape = outputs[0].GetStorageShape();
    auto out_dims = TensorTransUtils::GetDimsFromGertShape(out_shape);
    EXPECT_EQ(out_dims, shape_dim);
    return SUCCESS;
  };
  EXPECT_EQ(user_graph_manager.RunGraphAsync(user_graph_id, std::move(inputs), 0, callback), SUCCESS);
  // graph is built, no need build
  EXPECT_FALSE(user_graph_manager.IsGraphNeedRebuild(user_graph_id));

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  // graph is not exist, need rebuild
  EXPECT_TRUE(user_graph_manager.IsGraphNeedRebuild(user_graph_id));
  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, ExecuteGraphWithStreamAsync_Success) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options = {{ge::INPUT_HINT_SHAPE, "0:[2,3,4,5]"}};
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(1);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, nullptr, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT, return_compile_load_summary_execute_success_when_input_static_graph_not_partition) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // static shape graph
  EXPECT_EQ(summary->IsStatic(), true);
  std::vector<ge::Shape> output_shape;
  EXPECT_EQ(summary->GetOutputShapes(output_shape), ge::SUCCESS);
  std::vector<int64_t> expect_dims{1, 2, 3, 4};
  ASSERT_EQ(output_shape.size(), 1);
  EXPECT_EQ(output_shape[0].GetDims(), expect_dims);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, nullptr), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(1);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  gert_outputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}}, {}, {}, {}, nullptr};
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, nullptr, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT,
       return_compile_load_summary_not_null_execute_success_when_input_static_graph_contain_partition) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::OneReshapeNode({1, 2, 3, 4}, {4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());

  const std::map<std::string, std::string> options = {{ge::INPUT_HINT_SHAPE, "0:[2,3,4,5]; 1:[4]"}};
  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // dynamic shape graph
  EXPECT_EQ(summary->IsStatic(), false);
  std::vector<ge::Shape> output_shape;
  EXPECT_NE(summary->GetOutputShapes(output_shape), ge::SUCCESS);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, nullptr), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(2);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  std::vector<int64_t> input_data_2{1, 2, 3, 4, 0, 0, 0, 0};
  gert_inputs[1] = {{{4}, {4}},                                  // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT64,                                // data type
                    (void *)input_data_2.data()};
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, nullptr, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_outputs.clear();
  gert_outputs.resize(1);
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, nullptr, gert_inputs, gert_outputs, 0),
            SUCCESS);  // hint guard no compile
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT, return_load_fail_when_input_static_graph_not_partition_not_compile) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_EQ(summary, nullptr);

  std::map<AscendString, AscendString> load_options;
  EXPECT_NE(user_graph_manager.LoadGraph(user_graph_id, load_options, nullptr), SUCCESS);

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT, return_load_succ_when_input_dynamic_graph_partition_not_compile) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::OneReshapeNode();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_EQ(summary, nullptr);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, nullptr), SUCCESS);

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT,
       return_compile_load_summary_not_null_execute_success_when_input_static_graph_contain_partition_extern_stream) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::OneReshapeNode({1, 2, 3, 4}, {4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());

  const std::map<std::string, std::string> options;
  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // dynamic shape graph
  EXPECT_EQ(summary->IsStatic(), false);
  std::vector<ge::Shape> output_shape;
  EXPECT_NE(summary->GetOutputShapes(output_shape), ge::SUCCESS);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, new_stream), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(2);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  std::vector<int64_t> input_data_2{1, 2, 3, 4, 0, 0, 0, 0};
  gert_inputs[1] = {{{4}, {4}},                                  // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT64,                                // data type
                    (void *)input_data_2.data()};
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, new_stream, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_outputs.clear();
  gert_outputs.resize(1);
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, new_stream, gert_inputs, gert_outputs, 0),
            SUCCESS);  // hint guard no compile
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT,
       return_compile_load_summary_execute_success_when_input_static_graph_not_partition_extern_stream) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // static shape graph
  EXPECT_EQ(summary->IsStatic(), true);
  std::vector<ge::Shape> output_shape;
  EXPECT_EQ(summary->GetOutputShapes(output_shape), ge::SUCCESS);
  std::vector<int64_t> expect_dims{1, 2, 3, 4};
  ASSERT_EQ(output_shape.size(), 1);
  EXPECT_EQ(output_shape[0].GetDims(), expect_dims);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, new_stream), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(1);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  gert_outputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}}, {}, {}, {}, nullptr};
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, new_stream, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(
    UserGraphsManagerlUT,
    return_compile_load_summary_execute_success_when_input_static_graph_not_partition_extern_stream_external_output) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  auto relu1 = compute_graph->FindNode("Relu_1");
  std::vector<std::pair<ge::NodePtr, int32_t>> output_nodes{{relu1, 0}};
  compute_graph->SetGraphOutNodesInfo(output_nodes);
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // static shape graph
  EXPECT_EQ(summary->IsStatic(), true);
  std::vector<ge::Shape> output_shape;
  EXPECT_EQ(summary->GetOutputShapes(output_shape), ge::SUCCESS);
  std::vector<int64_t> expect_dims{1, 2, 3, 4};
  ASSERT_EQ(output_shape.size(), 1);
  EXPECT_EQ(output_shape[0].GetDims(), expect_dims);

  std::map<AscendString, AscendString> load_options;
  EXPECT_EQ(user_graph_manager.LoadGraph(user_graph_id, load_options, new_stream), SUCCESS);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(1);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  std::vector<uint8_t> output_data_1(96, 0xFF);
  gert_outputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                     {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                     gert::kOnDeviceHbm,                          // placement
                     ge::DT_INT32,                                // data type
                     (void *)output_data_1.data()};
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, new_stream, gert_inputs, gert_outputs, 0),
            SUCCESS);
  EXPECT_EQ(gert_outputs.size(), 1);
  EXPECT_EQ(gert_outputs[0].GetOriginShape(), gert::Shape({1, 2, 3, 4}));

  gert_inputs.clear();
  gert_outputs.clear();
  graph_manager.UnregisterExternalAllocator(new_stream);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT,
       return_compile_summary_execute_success_when_input_static_graph_not_partition_extern_stream) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  const std::map<std::string, std::string> options;
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(user_graph_manager.CompileGraph(user_graph_id, 0, {}), SUCCESS);

  CompiledGraphSummaryPtr summary;
  EXPECT_EQ(user_graph_manager.GetCompiledGraphSummary(user_graph_id, summary), SUCCESS);
  EXPECT_NE(summary, nullptr);
  // static shape graph
  EXPECT_EQ(summary->IsStatic(), true);
  std::vector<ge::Shape> output_shape;
  EXPECT_EQ(summary->GetOutputShapes(output_shape), ge::SUCCESS);
  std::vector<int64_t> expect_dims{1, 2, 3, 4};
  ASSERT_EQ(output_shape.size(), 1);
  EXPECT_EQ(output_shape[0].GetDims(), expect_dims);

  // prepare run task
  std::vector<gert::Tensor> gert_inputs;
  std::vector<gert::Tensor> gert_outputs;
  gert_inputs.resize(1);
  gert_outputs.resize(1);
  std::vector<int32_t> input_data_1(1 * 2 * 3 * 4, 666);
  gert_inputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}},                // shape
                    {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                    gert::kOnDeviceHbm,                          // placement
                    ge::DT_INT32,                                // data type
                    (void *)input_data_1.data()};
  gert_outputs[0] = {{{1, 2, 3, 4}, {1, 2, 3, 4}}, {}, {}, {}, nullptr};
  EXPECT_NE(user_graph_manager.ExecuteGraphWithStreamAsync(user_graph_id, new_stream, gert_inputs, gert_outputs, 0),
            SUCCESS);  // 未load报错

  gert_inputs.clear();
  gert_outputs.clear();
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
}

TEST_F(UserGraphsManagerlUT, set_memory_skip_by_slice_scheduler_enable) {
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true;--experimental_enable_jit_executor_v2=true", 1);  // 开启自动融合
  uint32_t graph_id = 1;
  std::map<AscendString, AscendString> options;
  EXPECT_EQ(GEInitialize(options), SUCCESS);
  Session session(options);
  dlog_setlevel(GE_MODULE_NAME, 0, 1);
  EXPECT_EQ(UNSUPPORTED, session.SetGraphConstMemoryBase(graph_id, nullptr, 0));
  EXPECT_EQ(UNSUPPORTED, session.UpdateGraphFeatureMemoryBase(graph_id, nullptr, 0));
  EXPECT_EQ(UNSUPPORTED,
            session.SetGraphFixedFeatureMemoryBaseWithType(graph_id, MemoryType::MEMORY_TYPE_DEFAULT, nullptr, 0));
  EXPECT_EQ(UNSUPPORTED, session.UpdateGraphRefreshableFeatureMemoryBase(graph_id, nullptr, 0));

  std::vector<std::string> expect_log_list = {
      "SetGraphConstMemoryBase does not support the slice scheduler currently",
      "UpdateGraphFeatureMemoryBase does not support the slice scheduler currently",
      "SetGraphFixedFeatureMemoryBaseWithType does not support the slice scheduler currently",
      "UpdateGraphRefreshableFeatureMemoryBase does not support the slice scheduler currently"};
  for (auto &it : expect_log_list) {
    EXPECT_NE(gert_stub_.GetSlogStub().FindLog(-1, it.c_str()), -1);
  }
  dlog_setlevel(GE_MODULE_NAME, 3, 1);
  EXPECT_EQ(GEFinalize(), SUCCESS);
  unsetenv("AUTOFUSE_FLAGS");
}

// 跳过断图调度的场景 - 降级到传统模式
TEST_F(UserGraphsManagerlUT, graph_skip_slice_schedule_dynamic_batch) {
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true;--experimental_enable_jit_executor_v2=true", 1);

  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  std::map<std::string, std::string> options;

  // 动态分档场景 - 不支持 slice schedule，降级到传统模式
  options["ge.dynamicDims"] = "1,1,1;2,2,2;3,3,3";
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UserGraphsManagerlUT, graph_skip_slice_schedule_aoe_mode) {
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true;--experimental_enable_jit_executor_v2=true", 1);

  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 1u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  std::map<std::string, std::string> options;

  // aoe 场景 - 不支持 slice schedule，降级到传统模式
  options["ge.buildMode"] = "tuning";
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UserGraphsManagerlUT, graph_skip_slice_schedule_unsupported_op) {
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true;--experimental_enable_jit_executor_v2=true", 1);

  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 2u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph);

  // 添加不支持算子（Switch）到图中
  auto switch_op = std::make_shared<OpDesc>("switch_node", "Switch");
  GeTensorDesc input_desc(GeShape({1, 2, 3, 4}), FORMAT_ND, DT_FLOAT);
  switch_op->AddInputDesc(input_desc);
  switch_op->AddInputDesc(input_desc);
  switch_op->AddOutputDesc(input_desc);
  switch_op->AddOutputDesc(input_desc);
  compute_graph->AddNode(switch_op);

  std::map<std::string, std::string> options;
  // 包含不支持算子 - 不支持 slice schedule，降级到传统模式
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UserGraphsManagerlUT, graph_skip_slice_schedule_resource_op) {
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true;--experimental_enable_jit_executor_v2=true", 1);

  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);
  rtStream_t new_stream;
  (void)aclrtCreateStreamWithConfig(&new_stream, 0, 0);

  uint32_t user_graph_id = 3u;
  auto graph = JitShareGraph::AllNormalNodes({1, 2, 3, 4});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph);

  // 添加资源算子（输出类型为 DT_RESOURCE）到图中
  auto resource_op = std::make_shared<OpDesc>("resource_node", "ResourceOp");
  GeTensorDesc input_desc(GeShape({1, 2, 3, 4}), FORMAT_ND, DT_FLOAT);
  GeTensorDesc output_desc(GeShape({1, 2, 3, 4}), FORMAT_ND);
  output_desc.SetOriginDataType(DT_RESOURCE);
  resource_op->AddInputDesc(input_desc);
  resource_op->AddOutputDesc(output_desc);
  compute_graph->AddNode(resource_op);

  std::map<std::string, std::string> options;
  // 包含资源算子 - 不支持 slice schedule，降级到传统模式
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);
  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);

  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
  aclrtDestroyStream(new_stream);
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UserGraphsManagerlUT, add_graph_verify_options_seperation) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  std::map<std::string, std::string> options;
  options["ge.inputShape"] = "1,2,3,4";
  options["ge.outputDatatype"] = "float16";
  options["another.middle"] = "another_value";
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  auto *ctrl = user_graph_manager.ids_to_user_graph_ctrl_[user_graph_id].get();
  ASSERT_NE(ctrl, nullptr);
  EXPECT_EQ(ctrl->order_.first_ep_options_.size(), 3U);
  EXPECT_EQ(ctrl->order_.middle_ep_options_.size(), 1U);
  EXPECT_EQ(ctrl->order_.last_ep_options_.size(), 2U);

  // Verify SelectEpOption before slicing: empty slice_graphs_ → first_ep_options_
  EXPECT_EQ(ctrl->order_.SelectEpOption({}), ctrl->order_.first_ep_options_);
  EXPECT_NE(ctrl->order_.SelectEpOption({}), ctrl->order_.middle_ep_options_);

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);
  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

TEST_F(UserGraphsManagerlUT, add_graph_verify_options_flow_to_ep_after_slicing) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::AllNormalNodes();
  std::map<std::string, std::string> options;
  options["ge.inputShape"] = "1,2,3,4";
  options["ge.outputDatatype"] = "float16";
  options["my.custom"] = "custom_val";
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  std::vector<int64_t> shape_dim = {2, 3, 3, 2};
  std::vector<gert::Tensor> inputs(1);
  TensorCheckUtils::ConstructGertTensor(inputs[0], {2, 3, 3, 2}, DT_FLOAT, FORMAT_NCHW);

  std::promise<Status> promise;
  auto future = promise.get_future();
  auto *ugm_ptr = &user_graph_manager;
  const RunAsyncCallbackV2 callback = [&](Status status, std::vector<gert::Tensor> &outputs) {
    EXPECT_EQ(status, SUCCESS);
    EXPECT_EQ(outputs.size(), 1);
    auto *ctrl = ugm_ptr->ids_to_user_graph_ctrl_[user_graph_id].get();
    if (outputs.empty() || ctrl == nullptr || ctrl->order_.slice_graphs_.empty()) {
      promise.set_value(FAILED);
      return FAILED;
    }
    auto out_dims = TensorTransUtils::GetDimsFromGertShape(outputs[0].GetStorageShape());
    EXPECT_EQ(out_dims, shape_dim);

    auto &first_ep = ctrl->order_.slice_graphs_.front();
    EXPECT_NE(first_ep->GetEpGraphOptions().find("ge.inputShape"), first_ep->GetEpGraphOptions().end());
    EXPECT_EQ(first_ep->GetEpGraphOptions().size(), 3U);

    if (ctrl->order_.slice_graphs_.size() > 1U) {
      auto &last_ep = ctrl->order_.slice_graphs_.back();
      EXPECT_EQ(last_ep->GetEpGraphOptions().find("ge.inputShape"), last_ep->GetEpGraphOptions().end());
      EXPECT_NE(last_ep->GetEpGraphOptions().find("ge.outputDatatype"), last_ep->GetEpGraphOptions().end());
    }

    promise.set_value(status);
    return SUCCESS;
  };
  EXPECT_EQ(user_graph_manager.RunGraphAsync(user_graph_id, std::move(inputs), 0, callback), SUCCESS);
  EXPECT_EQ(future.get(), SUCCESS);

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);
  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

static void VerifyEpOptions(const std::vector<std::unique_ptr<ge::ExecutionPoint>> &slice_graphs) {
  EXPECT_GT(slice_graphs.size(), 2U) << "should have first + middle + last EPs";

  auto &first_opts = slice_graphs.front()->GetEpGraphOptions();
  EXPECT_NE(first_opts.find("ge.inputShape"), first_opts.end());
  EXPECT_EQ(first_opts.size(), 3U);

  for (size_t i = 1; i < slice_graphs.size() - 1; ++i) {
    auto &mid_opts = slice_graphs[i]->GetEpGraphOptions();
    EXPECT_EQ(mid_opts.find("ge.inputShape"), mid_opts.end());
    EXPECT_EQ(mid_opts.find("ge.outputDatatype"), mid_opts.end());
    EXPECT_NE(mid_opts.find("my.custom"), mid_opts.end());
    EXPECT_EQ(mid_opts.size(), 1U);
  }

  auto &last_opts = slice_graphs.back()->GetEpGraphOptions();
  EXPECT_EQ(last_opts.find("ge.inputShape"), last_opts.end());
  EXPECT_NE(last_opts.find("ge.outputDatatype"), last_opts.end());
  EXPECT_NE(last_opts.find("my.custom"), last_opts.end());
  EXPECT_EQ(last_opts.size(), 2U);
}

TEST_F(UserGraphsManagerlUT, add_graph_verify_multi_ep_options_seperation) {
  ModelExecutor model_executor;
  model_executor.Initialize({}, 0);
  GraphManager graph_manager;
  EXPECT_EQ(graph_manager.Initialize({}, &model_executor), SUCCESS);
  UserGraphsManager user_graph_manager(graph_manager);

  uint32_t user_graph_id = 0u;
  auto graph = JitShareGraph::TwoReshapeNodeTwoRelu();
  std::map<std::string, std::string> options;
  options["ge.inputShape"] = "1,2,3,4";
  options["ge.outputDatatype"] = "float16";
  options["my.custom"] = "custom_val";
  EXPECT_EQ(user_graph_manager.AddGraph(user_graph_id, *graph, options), SUCCESS);

  std::vector<float> data0(2 * 3 * 3 * 2, 0.0f);
  std::vector<int64_t> shape_data{2, 3, 3, 2};
  std::vector<gert::Tensor> inputs(2);
  inputs[0] = {{{2, 3, 3, 2}, {2, 3, 3, 2}},
               {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},
               gert::kOnDeviceHbm,
               ge::DT_FLOAT,
               data0.data()};
  inputs[1] = {
      {{4}, {4}}, {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}}, gert::kOnDeviceHbm, ge::DT_INT64, shape_data.data()};

  std::promise<Status> promise;
  auto future = promise.get_future();
  auto *ugm_ptr = &user_graph_manager;
  const RunAsyncCallbackV2 callback = [&](Status status, std::vector<gert::Tensor> &outputs) {
    EXPECT_EQ(status, SUCCESS);
    auto *ctrl = ugm_ptr->ids_to_user_graph_ctrl_[user_graph_id].get();
    if (outputs.empty() || ctrl == nullptr || ctrl->order_.slice_graphs_.empty()) {
      promise.set_value(FAILED);
      return FAILED;
    }
    VerifyEpOptions(ctrl->order_.slice_graphs_);
    promise.set_value(status);
    return SUCCESS;
  };
  EXPECT_EQ(user_graph_manager.RunGraphAsync(user_graph_id, std::move(inputs), 0, callback), SUCCESS);
  EXPECT_EQ(future.get(), SUCCESS);

  EXPECT_EQ(user_graph_manager.RemoveGraph(user_graph_id), SUCCESS);
  EXPECT_EQ(user_graph_manager.Finalize(), SUCCESS);
  EXPECT_EQ(graph_manager.Finalize(), SUCCESS);
}

}  // namespace ge
