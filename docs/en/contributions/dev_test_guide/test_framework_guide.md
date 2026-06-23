# Test Framework Guide

> Back to [DT Case Development Guide](README.md)

UT/ST development has some test capabilities that are tedious and common. These tasks include: constructing compute graph etc. inputs, taking over rts etc. low-level interfaces, verifying a graph's correctness etc. These code aren't difficult but verbose. If directly written in cases, affects case readability and causes lots of duplicate code between cases. Therefore in test framework, provided faker, stub, checker etc. mechanisms, separately for constructing inputs, stubbing low-level interfaces, verifying outputs.

This section introduces various capabilities already provided in test framework, letting you quickly know if current test framework already provides this functionality when having a new public capability requirement. Therefore only overview introduction here. If need detailed usage instructions, please refer to corresponding framework capability header files and documentation.

## Test Base Classes

Test base classes inherit from gtest's `testing::Test`, and do some common initialization and cleanup actions in SetUp and TearDown, to reduce case redundant code. Currently encapsulated test base classes:

* BgTest: No setup processing, TearDown automatically clears all ValueHolder's Frame
* BgTestAutoCreateFrame: Setup automatically creates a root Frame, TearDown automatically clears all ValueHolder's Frame
* BgTestAutoCreate3StageFrame: Setup automatically creates Init, Main, DeInit three node graphs, and selects current Frame as Main graph Frame; TearDown automatically clears all ValueHolder's Frame

> Above base class header files: `#include "common/bg_test.h"`

## Faker Introduction

Faker used to construct some data, these data can conveniently be used for UT or ST testing. Currently supported fakers:

### Graph DSL (Declarative Graph Construction)

`#include "ge_graph_dsl/graph_dsl.h"`

Graph DSL based on easy_graph library encapsulation, provides macro-driven graph building approach, replacing verbose manual graph construction code. Core capabilities:

* **Core macros**: `DEF_GRAPH(name) { ... }` defines graph, `CHAIN(...)` defines a node-edge-node chain, `NODE("name", op_type)` creates node, `EDGE(src_out_idx, dst_in_idx)` creates data edge
* **Operator configuration**: `OP_CFG(op_type)` creates streaming operator description configuration, supports `.TensorDesc()`, `.Weight()`, `.Attr()`, `.InputAttr()`, `.OutputAttr()`, `.InCnt()`, `.OutCnt()` etc. chain calls
* **Graph conversion**: `ToGeGraph(graph)` → `ge::Graph`, `ToComputeGraph(graph)` → `ComputeGraphPtr`, `ToExecuteGraph(graph)` → `ExecuteGraphPtr`
* **Graph assertion**: `DUMP_GRAPH_WHEN("phase1", ...)` captures graph snapshot at specified compilation phase, `CHECK_GRAPH(phase_id) { ... }` executes assertion verification on snapshot (detailed usage see [ST Development Guide](./ST_testcase_dev_guide.md))

Usage example:
```c++
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_graph_dsl/assert/graph_assert.h"

TEST_F(MyUT, BuildAndCheckGraph) {
  // Configure operators
  auto data_cfg = OP_CFG(DATA).TensorDesc(FORMAT_NCHW, DT_FLOAT, {1, 224, 224, 224});
  auto add_cfg = OP_CFG(ADD).TensorDesc(FORMAT_NCHW, DT_FLOAT, {1, 224, 224, 224});

  // Declarative graph construction
  DEF_GRAPH(g) {
    CHAIN(NODE("data_0", data_cfg)->EDGE(0, 0)->NODE("add", add_cfg));
    CHAIN(NODE("data_1", data_cfg)->EDGE(0, 1)->NODE("add", add_cfg));
    CHAIN(NODE("add", add_cfg)->NODE("netoutput", NETOUTPUT));
  };

  // Convert to GE graph
  auto compute_graph = ToComputeGraph(g);
  ASSERT_NE(compute_graph, nullptr);
}
```

### Other Fakers

* NodeFaker: Fake a ge::NodePtr object `#include "faker/node_faker.h"`
* TensorFaker: Fake one or multiple gert::Tensor `#include "faker/fake_value.h"`
* KernelRunContextFaker: Fake KernelContext, this is a series of faker, besides regular KernelContext, can also fake TilingContext, InferShapeContext `#include "faker/kernel_run_context_facker.h"`
* GeModelBuilder: Accept a compute graph object, fake it as Model `#include "faker/ge_model_builder.h"`
* ModelDataFaker: Accept a model, fake it as ModelData `#include "faker/model_data_faker.h"`
* GlobalDataFaker: Fake a LoweringGlobalData `#include "faker/global_data_faker.h"`
* MagicOpFaker: Fake an operator's complete implementation, including Lowering and complete kernel `#include "faker/magic_ops.h"`

## Stub Introduction

Stub stubs and takes over runtime environment, e.g., slog, rts etc., also provides interfaces for cases to verify GE's calling behavior correctness from log, rts perspective. For compile time, less dependency on low-level, therefore almost no need for stubbing.

For execution time, stub has a package solution: `GertRuntimeStub` (`#include "stub/gert_runtime_stub.h"`). Once this class instantiated, automatically stubs all rts interfaces, takes over slog to listen logs, takes over node converter and kernel registry. Then in case can operate this instance (only affects this case behavior, won't affect global other cases).

For example in some case, we expect to verify a log:
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

## Checker Introduction

Checker mainly concentrated in graph verification. Currently provided checker capabilities:

* SummaryChecker: Verify a graph's overall info, e.g., how many nodes, count for each node type `#include "common/summary_checker.h"`
* TopoChecker: Verify topology info, can conveniently verify how many outputs or inputs some node has, can chain verify a string of nodes' edge relationships `#include "common/topo_checker.h"`

## Test Framework Improvement

Test framework also maintained by developers. And currently test framework not assigned as separate responsibility field, therefore encourage anyone to actively improve test framework. Improvement items include but not limited to: new faker/stub/checker etc. framework capabilities, fix existing test framework bugs, improve usability etc.