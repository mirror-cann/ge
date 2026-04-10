# 测试框架指南

> 返回 [DT用例开发总纲](README.md)

UT/ST开发时有一些测试能力是繁琐且公用的，这些工作包括：构造计算图等输入、接管rts等底层接口、校验一张图的正确性等。这些代码难度不高，但是很冗长，如果直接在用例中编写会影响用例的可读性，并且导致用例之间存在大量重复代码。因此在测试框架中，提供了faker、stub、checker等机制，分别用于构造输入、打桩底层接口、校验输出。

本节介绍测试框架中已经提供的各种能力，让您在有一个新的公共能力需求时，可以快速了解当前测试框架是否已经提供了这个功能。因此这里仅做概要介绍，如果需要详细的使用说明，请参考对应框架能力的头文件和文档。

## 测试基类

测试基类继承自gtest的`testing::Test`，并在SetUp和TearDown中做一些公共的初始化和清理动作，以减少用例的冗余代码。当前封装的测试基类有：

* BgTest：无setup处理，TearDown时自动清空所有ValueHolder的Frame
* BgTestAutoCreateFrame：setup时自动创建一个根Frame，TearDown时自动清空所有ValueHolder的Frame
* BgTestAutoCreate3StageFrame：setup时自动创建好Init、Main、DeInit三个节点的图，并将当前的Frame选择为Main图的Frame；TearDown时自动清空所有ValueHolder的Frame

> 以上基类的头文件：`#include "common/bg_test.h"`

## faker介绍

faker用于构造一些数据，这些数据可以方便地用于UT或ST测试，当前支持的faker有：

### Graph DSL（声明式构图）

`#include "ge_graph_dsl/graph_dsl.h"`

Graph DSL 基于 easy_graph 库封装，提供了宏驱动的图构建方式，替代冗长的手动构图代码。核心能力如下：

* **核心宏**：`DEF_GRAPH(name) { ... }` 定义图，`CHAIN(...)` 定义一条节点-边-节点的链路，`NODE("name", op_type)` 创建节点，`EDGE(src_out_idx, dst_in_idx)` 创建数据边
* **算子配置**：`OP_CFG(op_type)` 创建流式算子描述配置，支持 `.TensorDesc()`、`.Weight()`、`.Attr()`、`.InputAttr()`、`.OutputAttr()`、`.InCnt()`、`.OutCnt()` 等链式调用
* **图转换**：`ToGeGraph(graph)` → `ge::Graph`，`ToComputeGraph(graph)` → `ComputeGraphPtr`，`ToExecuteGraph(graph)` → `ExecuteGraphPtr`
* **图断言**：`DUMP_GRAPH_WHEN("phase1", ...)` 在指定编译阶段捕获图快照，`CHECK_GRAPH(phase_id) { ... }` 对快照执行断言校验（详细用法见 [ST用例开发指导](ST用例开发指导.md)）

使用示例：
```c++
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_graph_dsl/assert/graph_assert.h"

TEST_F(MyUT, BuildAndCheckGraph) {
  // 配置算子
  auto data_cfg = OP_CFG(DATA).TensorDesc(FORMAT_NCHW, DT_FLOAT, {1, 224, 224, 224});
  auto add_cfg = OP_CFG(ADD).TensorDesc(FORMAT_NCHW, DT_FLOAT, {1, 224, 224, 224});

  // 声明式构图
  DEF_GRAPH(g) {
    CHAIN(NODE("data_0", data_cfg)->EDGE(0, 0)->NODE("add", add_cfg));
    CHAIN(NODE("data_1", data_cfg)->EDGE(0, 1)->NODE("add", add_cfg));
    CHAIN(NODE("add", add_cfg)->NODE("netoutput", NETOUTPUT));
  };

  // 转换为GE图
  auto compute_graph = ToComputeGraph(g);
  ASSERT_NE(compute_graph, nullptr);
}
```

### 其他faker

* NodeFaker：仿冒一个ge::NodePtr对象 `#include "faker/node_faker.h"`
* TensorFaker：仿冒一个或多个gert::Tensor `#include "faker/fake_value.h"`
* KernelRunContextFaker：仿冒KernelContext，这是一个系列的faker，除了常规的KernelContext，还可以仿冒TilingContext、InferShapeContext `#include "faker/kernel_run_context_facker.h"`
* GeModelBuilder：接受一个计算图对象，将其仿冒为Model `#include "faker/ge_model_builder.h"`
* ModelDataFaker：接受一个model，将其仿冒为ModelData `#include "faker/model_data_faker.h"`
* GlobalDataFaker：仿冒一个LoweringGlobalData `#include "faker/global_data_faker.h"`
* MagicOpFaker：仿冒一个算子的完整实现，包括Lowering及其完整kernel `#include "faker/magic_ops.h"`

## stub介绍

stub打桩和接管运行时环境，例如slog、rts等，同时提供了接口，可供用例中验证从日志、rts等视角，GE的调用行为是否正确。对于编译时来说，对底层的依赖较少，因此几乎不需要打桩。

对于执行时来说，stub有个一揽子解决方案：`GertRuntimeStub`（`#include "stub/gert_runtime_stub.h"`）。该类一旦被实例化，则自动打桩所有的rts接口，接管slog监听日志，接管node converter和kernel的registry。接下来用例中可以操作该实例（仅影响本用例行为，不会影响到全局的其他用例）。

例如在某个用例中，我们期望校验一条日志：
```c++
TEST_F(KernelLogUT, KLog_Success_LogError) {
  GertRuntimeStub stub;
  auto context_holder = KernelRunContextFaker().KernelName("tn").KernelType("tt").Build();
  auto context = context_holder.GetContext<KernelContext>();

  stub.GetSlogStub().Clear();
  KLOGE("Hello world");
  KLOGE("Hello world %d", 123);

  ASSERT_EQ(stub.GetSlogStub().GetLogs().size(), 2);
  ASSERT_EQ(stub.GetSlogStub().GetLogs().at(0).level, DLOG_ERROR);
  ASSERT_EQ(stub.GetSlogStub().GetLogs().at(1).level, DLOG_ERROR);
  ASSERT_ENDSWITH(stub.GetSlogStub().GetLogs().at(0).content, "[tt][tn]Hello world");
  ASSERT_ENDSWITH(stub.GetSlogStub().GetLogs().at(1).content, "[tt][tn]Hello world 123");
}
```

## checker介绍

checker主要集中在图校验中，当前提供了如下checker能力：

* SummaryChecker：校验一张图的整体信息，例如有多少节点，每类节点的个数等 `#include "common/summary_checker.h"`
* TopoChecker：校验拓扑信息，可方便地校验某个node有多少输出或输入，可以链式校验一串node的连边关系 `#include "common/topo_checker.h"`

## 测试框架的改进

测试框架也是由开发人员维护的，而且目前测试框架没有被划为单独的责任田，因此鼓励任何人主动改进测试框架，改进项包含但不限于：新增faker/stub/checker等框架能力、修复测试框架已有bug、提升易用性等等。
