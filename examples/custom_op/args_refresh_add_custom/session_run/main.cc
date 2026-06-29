/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <random>
#include <vector>

#include "acl/acl_rt.h"
#include "ge/ge_api.h"
#include "graph.h"
#include "ops_proto_legacy.h"
#include "tensor.h"
#include "types.h"
#include "add_custom.h"
#include "utils/log.h"

using ge::Operator;

namespace {
/**
 * @brief 常量定义
 *
 * kRefreshGraphId: 使用 ArgsUpdater 的图 ID
 * kNoRefreshGraphId: 不使用 ArgsUpdater 的图 ID
 * kDim0/kDim1: 输入张量维度 [4096, 4096]
 * kNumElements: 总元素数量
 * kDataSizeBytes: 数据总字节数 (64MB)
 * kWarmupIters: 预热次数，用于稳定性能
 * kBenchIters: 性能测试次数
 */
constexpr uint32_t kRefreshGraphId = 0U;
constexpr uint32_t kNoRefreshGraphId = 1U;
constexpr int64_t kDim0 = 4096;
constexpr int64_t kDim1 = 4096;
constexpr int64_t kNumElements = kDim0 * kDim1;
constexpr size_t kDataSizeBytes = static_cast<size_t>(kNumElements) * sizeof(float);
constexpr int kWarmupIters = 5;
constexpr int kBenchIters = 100;
constexpr int kMemorySets = 2;
constexpr int kNumInputs = 2;
constexpr int kRandomSeed = 42;
constexpr int kMaxErrorPrints = 10;
constexpr float kErrorTolerance = 1e-5f;
constexpr size_t kBytesPerMB = 1024 * 1024;

/**
 * @brief 构建计算图
 *
 * 根据 use_refresh 参数选择使用 AddRefreshOp（带 ArgsUpdater）或 AddNoRefreshOp（不带 ArgsUpdater）
 * 两个算子功能相同，区别在于是否支持地址刷新优化
 *
 * @param name 图名称
 * @param op_name 算子名称
 * @param use_refresh 是否使用 ArgsUpdater
 * @return 构建好的图对象
 */
std::unique_ptr<ge::Graph> BuildGraph(const char *name, const char *op_name, bool use_refresh) {
  // 创建输入张量描述：[4096, 4096] 的 float32 数据，ND 格式
  ge::TensorDesc input_desc(ge::Shape({kDim0, kDim1}), ge::FORMAT_ND, ge::DT_FLOAT);

  // 创建两个 Data 算子作为图的输入
  auto data_x = ge::op::Data("data_x");
  data_x.update_input_desc_x(input_desc);
  data_x.update_output_desc_y(input_desc);
  auto data_y = ge::op::Data("data_y");
  data_y.update_input_desc_x(input_desc);
  data_y.update_output_desc_y(input_desc);

  std::vector<Operator> inputs = {data_x, data_y};
  std::vector<Operator> outputs;

  // 根据参数选择算子类型
  if (use_refresh) {
    auto add = ge::op::AddRefreshOp(op_name).set_input_x(data_x).set_input_y(data_y);
    outputs.push_back(add);
  } else {
    auto add = ge::op::AddNoRefreshOp(op_name).set_input_x(data_x).set_input_y(data_y);
    outputs.push_back(add);
  }

  auto graph = std::make_unique<ge::Graph>(name);
  graph->SetInputs(inputs).SetOutputs(outputs);
  return graph;
}

/**
 * @brief 分配设备内存
 *
 * 在 NPU 上分配 kDataSizeBytes (64MB) 的设备内存
 * 使用 ACL_MEM_MALLOC_HUGE_FIRST 策略优先分配大页内存
 *
 * @return 分配的设备内存指针，失败返回 nullptr
 */
void *AllocDeviceMemory() {
  void *dev_ptr = nullptr;
  aclrtMalloc(&dev_ptr, kDataSizeBytes, ACL_MEM_MALLOC_HUGE_FIRST);
  return dev_ptr;
}

/**
 * @brief 释放设备内存
 *
 * @param ptr 要释放的设备内存指针
 */
void FreeDeviceMemory(void *ptr) {
  if (ptr != nullptr) {
    aclrtFree(ptr);
  }
}

/**
 * @brief 构建设备输入张量
 *
 * 将设备内存指针包装成 gert::Tensor 对象，用于传递给 GE 框架
 *
 * @param x_ptr 输入 x 的设备内存指针
 * @param y_ptr 输入 y 的设备内存指针
 * @return 包含两个输入张量的向量
 */
std::vector<gert::Tensor> BuildDeviceInputTensors(void *x_ptr, void *y_ptr) {
  std::vector<gert::Tensor> inputs(kNumInputs);
  inputs[0] = {
      {{kDim0, kDim1}, {kDim0, kDim1}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, gert::kOnDeviceHbm, ge::DT_FLOAT, x_ptr};
  inputs[1] = {
      {{kDim0, kDim1}, {kDim0, kDim1}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, gert::kOnDeviceHbm, ge::DT_FLOAT, y_ptr};
  return inputs;
}

/**
 * @brief 构建设备输出张量
 *
 * 将设备内存指针包装成 gert::Tensor 对象，用于接收计算结果
 *
 * @param z_ptr 输出 z 的设备内存指针
 * @return 包含一个输出张量的向量
 */
std::vector<gert::Tensor> BuildDeviceOutputTensors(void *z_ptr) {
  std::vector<gert::Tensor> outputs(1);
  outputs[0] = {
      {{kDim0, kDim1}, {kDim0, kDim1}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, gert::kOnDeviceHbm, ge::DT_FLOAT, z_ptr};
  return outputs;
}

/**
 * @brief 验证计算结果精度
 *
 * 逐元素比较 host_z[i] 与预期值 (host_x[i] + host_y[i])
 * 容差阈值为 1e-5，超过阈值的元素计为错误
 *
 * @param host_x 输入 x 的主机数据
 * @param host_y 输入 y 的主机数据
 * @param host_z 输出 z 的主机数据（从设备拷贝回来）
 * @return true 表示精度校验通过，false 表示失败
 */
bool VerifyResult(const std::vector<float> &host_x, const std::vector<float> &host_y,
                  const std::vector<float> &host_z) {
  int error_count = 0;
  float max_error = 0.0f;

  // 逐元素计算误差
  for (int64_t i = 0; i < kNumElements; ++i) {
    float expected = host_x[i] + host_y[i];
    float error = std::abs(host_z[i] - expected);
    max_error = std::max(max_error, error);

    // 超过容差阈值则计为错误
    if (error > kErrorTolerance) {
      error_count++;
      if (error_count <= kMaxErrorPrints) {
        LOG_ERROR("Error at [", i, "]: expected=", expected, ", got=", host_z[i]);
      }
    }
  }

  // 输出校验结果
  if (error_count > 0) {
    LOG_ERROR("Precision check failed: ", error_count, " errors, max_error=", max_error);
    return false;
  }
  LOG_INFO("Precision check passed, max_error=", max_error);
  return true;
}

/**
 * @brief 性能测试上下文，持有测试所需的所有状态
 */
struct BenchmarkContext {
  void *x_ptrs[kMemorySets];
  void *y_ptrs[kMemorySets];
  void *z_ptrs[kMemorySets];
  std::vector<float> host_x;
  std::vector<float> host_y;
  std::vector<float> host_z;
  std::vector<gert::Tensor> inputs_set[kMemorySets];
  std::vector<gert::Tensor> outputs_set[kMemorySets];

  /**
   * @brief 初始化上下文：分配内存、准备数据、构建 tensor
   * @return true 表示成功，false 表示失败
   */
  bool Init() {
    x_ptrs[0] = AllocDeviceMemory();
    x_ptrs[1] = AllocDeviceMemory();
    y_ptrs[0] = AllocDeviceMemory();
    y_ptrs[1] = AllocDeviceMemory();
    z_ptrs[0] = AllocDeviceMemory();
    z_ptrs[1] = AllocDeviceMemory();

    // 检查内存分配是否成功
    for (int i = 0; i < kMemorySets; ++i) {
      if (x_ptrs[i] == nullptr || y_ptrs[i] == nullptr || z_ptrs[i] == nullptr) {
        LOG_ERROR("Failed to allocate device memory for set ", i);
        return false;
      }
    }

    LOG_INFO("Memory set 0: x=", x_ptrs[0], " y=", y_ptrs[0], " z=", z_ptrs[0]);
    LOG_INFO("Memory set 1: x=", x_ptrs[1], " y=", y_ptrs[1], " z=", z_ptrs[1]);

    host_x.resize(kNumElements);
    host_y.resize(kNumElements);
    host_z.resize(kNumElements);

    std::mt19937 rng(kRandomSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int64_t i = 0; i < kNumElements; ++i) {
      host_x[i] = dist(rng);
      host_y[i] = dist(rng);
    }

    for (int s = 0; s < kMemorySets; ++s) {
      aclError ret = aclrtMemcpy(x_ptrs[s], kDataSizeBytes, host_x.data(), kDataSizeBytes, ACL_MEMCPY_HOST_TO_DEVICE);
      if (ret != ACL_ERROR_NONE) {
        LOG_ERROR("Failed to copy x data to device for set ", s, ", error: ", ret);
        return false;
      }
      ret = aclrtMemcpy(y_ptrs[s], kDataSizeBytes, host_y.data(), kDataSizeBytes, ACL_MEMCPY_HOST_TO_DEVICE);
      if (ret != ACL_ERROR_NONE) {
        LOG_ERROR("Failed to copy y data to device for set ", s, ", error: ", ret);
        return false;
      }
    }

    inputs_set[0] = BuildDeviceInputTensors(x_ptrs[0], y_ptrs[0]);
    inputs_set[1] = BuildDeviceInputTensors(x_ptrs[1], y_ptrs[1]);
    outputs_set[0] = BuildDeviceOutputTensors(z_ptrs[0]);
    outputs_set[1] = BuildDeviceOutputTensors(z_ptrs[1]);

    return true;
  }

  /**
   * @brief 释放所有设备内存
   */
  void Cleanup() {
    for (int j = 0; j < kMemorySets; ++j) {
      FreeDeviceMemory(x_ptrs[j]);
      FreeDeviceMemory(y_ptrs[j]);
      FreeDeviceMemory(z_ptrs[j]);
    }
  }
};

/**
 * @brief 执行预热轮次
 *
 * 交替使用两组内存执行 warmup 轮，稳定性能
 *
 * @param session GE 会话对象
 * @param graph_id 要测试的图 ID
 * @param stream 执行流
 * @param ctx 性能测试上下文
 * @param warmup 预热轮数
 * @return true 表示成功，false 表示失败
 */
bool RunWarmup(ge::Session &session, uint32_t graph_id, aclrtStream stream, BenchmarkContext &ctx, int warmup) {
  for (int i = 0; i < warmup; ++i) {
    int s = i % kMemorySets;
    const auto ret = session.ExecuteGraphWithStreamAsync(graph_id, stream, ctx.inputs_set[s], ctx.outputs_set[s]);
    if (ret != ge::SUCCESS) {
      LOG_ERROR("ExecuteGraphWithStreamAsync warmup failed, ret: ", ret);
      return false;
    }
  }
  aclrtSynchronizeStream(stream);
  return true;
}

/**
 * @brief 执行性能测试并计时
 *
 * 交替使用两组内存执行 iters 次，测量总耗时
 *
 * @param session GE 会话对象
 * @param graph_id 要测试的图 ID
 * @param stream 执行流
 * @param ctx 性能测试上下文
 * @param iters 测试次数
 * @return 总耗时（微秒），失败返回 -1.0
 */
double RunBenchmark(ge::Session &session, uint32_t graph_id, aclrtStream stream, BenchmarkContext &ctx, int iters) {
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) {
    int s = i % kMemorySets;
    const auto ret = session.ExecuteGraphWithStreamAsync(graph_id, stream, ctx.inputs_set[s], ctx.outputs_set[s]);
    if (ret != ge::SUCCESS) {
      LOG_ERROR("ExecuteGraphWithStreamAsync bench failed at iter ", i, ", ret: ", ret);
      return -1.0;
    }
  }
  aclrtSynchronizeStream(stream);
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::micro>(end - start).count();
}

/**
 * @brief 对指定图进行性能测试
 *
 * 执行流程：
 * 1. 初始化上下文（分配内存、准备数据、构建 tensor）
 * 2. 执行 warmup 次预热（交替使用两组内存）
 * 3. 执行 iters 次性能测试并计时（交替使用两组内存）
 * 4. 验证计算结果精度
 * 5. 释放设备内存
 *
 * 两组内存交替使用可以触发 UpdateHostArgs 中的地址变化，
 * 更真实地体现 ArgsUpdater 的优化效果。
 *
 * @param session GE 会话对象
 * @param graph_id 要测试的图 ID
 * @param stream 执行流
 * @param warmup 预热次数
 * @param iters 测试次数
 * @return 总耗时（微秒），失败返回 -1.0
 */
double BenchmarkGraph(ge::Session &session, uint32_t graph_id, aclrtStream stream, int warmup, int iters) {
  BenchmarkContext ctx;
  if (!ctx.Init()) {
    LOG_ERROR("Failed to initialize benchmark context");
    return -1.0;
  }

  if (!RunWarmup(session, graph_id, stream, ctx, warmup)) {
    ctx.Cleanup();
    return -1.0;
  }

  const double total_us = RunBenchmark(session, graph_id, stream, ctx, iters);
  if (total_us < 0.0) {
    ctx.Cleanup();
    return -1.0;
  }

  aclrtMemcpy(ctx.host_z.data(), kDataSizeBytes, ctx.z_ptrs[(iters - 1) % kMemorySets], kDataSizeBytes,
              ACL_MEMCPY_DEVICE_TO_HOST);
  VerifyResult(ctx.host_x, ctx.host_y, ctx.host_z);

  ctx.Cleanup();
  return total_us;
}

/**
 * @brief 设置并加载图
 *
 * 执行图的完整生命周期：添加 -> 编译 -> 加载
 *
 * @param session GE 会话对象
 * @param graph_id 图 ID
 * @param graph 图对象
 * @param stream 执行流
 * @param name 图名称（用于日志）
 * @return true 表示成功，false 表示失败
 */
bool SetupGraph(ge::Session &session, uint32_t graph_id, const std::unique_ptr<ge::Graph> &graph, aclrtStream stream,
                const char *name) {
  // 添加图到会话
  auto ret = session.AddGraph(graph_id, *graph);
  if (ret != ge::SUCCESS) {
    LOG_ERROR("AddGraph ", name, " failed, ret: ", ret);
    return false;
  }

  // 编译图
  ret = session.CompileGraph(graph_id);
  if (ret != ge::SUCCESS) {
    LOG_ERROR("CompileGraph ", name, " failed, ret: ", ret);
    return false;
  }

  // 加载图到设备
  std::map<ge::AscendString, ge::AscendString> load_options;
  ret = session.LoadGraph(graph_id, load_options, stream);
  if (ret != ge::SUCCESS) {
    LOG_ERROR("LoadGraph ", name, " failed, ret: ", ret);
    return false;
  }
  return true;
}
}  // namespace

/**
 * @brief 运行性能对比测试
 *
 * 对比 AddRefreshOp（带 ArgsUpdater）和 AddNoRefreshOp（不带 ArgsUpdater）的性能
 * 输出每轮平均耗时和加速比
 *
 * @param session GE 会话对象
 * @param stream 执行流
 * @return 0 表示成功，1 表示失败
 */
int RunPerformanceComparison(ge::Session &session, aclrtStream stream) {
  LOG_INFO("[Perf] input shape: [", kDim0, ", ", kDim1, "], float32, ", kDataSizeBytes / kBytesPerMB, "MB");
  LOG_INFO("[Perf] iters: ", kBenchIters);

  // 分别测试两个图的性能
  const double refresh_us = BenchmarkGraph(session, kRefreshGraphId, stream, kWarmupIters, kBenchIters);
  const double no_refresh_us = BenchmarkGraph(session, kNoRefreshGraphId, stream, kWarmupIters, kBenchIters);
  // 检查测试是否成功
  if (refresh_us < 0.0 || no_refresh_us < 0.0) {
    return 1;
  }

  // 计算并输出性能对比结果
  const double refresh_avg = refresh_us / kBenchIters;
  const double no_refresh_avg = no_refresh_us / kBenchIters;
  const double speedup = no_refresh_us / refresh_us;

  LOG_INFO("[Perf] With    ArgsUpdater: ", refresh_us, " us (avg ", refresh_avg, " us/iter)");
  LOG_INFO("[Perf] Without ArgsUpdater: ", no_refresh_us, " us (avg ", no_refresh_avg, " us/iter)");
  LOG_INFO("[Perf] Speedup: ", speedup, " x");

  return 0;
}

/**
 * @brief 主函数
 *
 * 执行流程：
 * 1. 初始化 GE 框架和 ACL
 * 2. 构建两个计算图（带/不带 ArgsUpdater）
 * 3. 编译并加载图
 * 4. 运行性能对比测试
 * 5. 清理资源
 */
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // GE 初始化选项
  // ge.exec.deviceId: 指定使用的 NPU 设备 ID
  // ge.graphRunMode: 1 表示 PRIORITY_GRAPH 模式，确保走在线执行链路
  std::map<ge::AscendString, ge::AscendString> options = {
      {"ge.exec.deviceId", "0"},
      {"ge.graphRunMode", "1"},
  };

  // 初始化 GE 框架
  const auto init_ret = ge::GEInitialize(options);
  if (init_ret != ge::SUCCESS) {
    LOG_ERROR("GEInitialize failed, ret: ", init_ret);
    return 1;
  }

  // 创建 ACL 执行流
  aclrtStream stream = nullptr;
  aclError acl_ret = aclrtCreateStream(&stream);
  if (acl_ret != ACL_ERROR_NONE) {
    LOG_ERROR("Failed to create stream, ret: ", acl_ret);
    return 1;
  }

  int ret_code = 0;
  {
    // 创建 GE 会话
    ge::Session session(options);

    // 构建两个计算图：一个带 ArgsUpdater，一个不带
    auto refresh_graph = BuildGraph("refresh_graph", "add_refresh", true);
    auto no_refresh_graph = BuildGraph("no_refresh_graph", "add_no_refresh", false);
    // 编译并加载两个图
    if (!SetupGraph(session, kRefreshGraphId, refresh_graph, stream, "refresh") ||
        !SetupGraph(session, kNoRefreshGraphId, no_refresh_graph, stream, "no_refresh")) {
      ret_code = 1;
    }

    // 运行性能对比测试
    if (ret_code == 0) {
      ret_code = RunPerformanceComparison(session, stream);
    }

    // 移除图
    (void)session.RemoveGraph(kRefreshGraphId);
    (void)session.RemoveGraph(kNoRefreshGraphId);
  }

  // 销毁执行流
  acl_ret = aclrtDestroyStream(stream);
  if (acl_ret != ACL_ERROR_NONE) {
    LOG_ERROR("Failed to destroy stream, ret: ", acl_ret);
  }

  // 清理 GE 框架
  const auto finalize_ret = ge::GEFinalize();
  if (finalize_ret != ge::SUCCESS) {
    LOG_ERROR("GEFinalize failed, ret: ", finalize_ret);
    return 1;
  }
  return ret_code;
}
