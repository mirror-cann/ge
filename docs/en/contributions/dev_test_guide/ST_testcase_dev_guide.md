# ST Case Development Guide

> Return to [DT Test Case Development Guide](README.md)

ST's testing scope is a feature, enabling and testing a feature from a large module perspective (such as compile-time, runtime), and observing whether compilation/execution results meet expectations. ST cases correspond to STC, so compared to UT cases, ST cases are more formal and correspondingly fewer in number.

## ST Case Annotation Requirements

Each ST needs to correspond to an STC design. ST annotations are the STC's description of the case. During requirement design, STC is carried via excel and other methods. After coding is complete, STC design is carried through ST case annotations. Subsequent changes and additions to ST cases should synchronously modify corresponding annotations. STC original excel files are no longer maintained. ST case annotations need to include four parts:

* Case description: One sentence describing this case's test scenario
* Preconditions: How to construct the ST environment, which basic environment data to fake and stub
* Test steps: Brief description of test steps
* Expected results: What results are expected. If there are multiple items, write them separately

For example, if you want to test whether the If node executes normally in rt2, you might have the following ST case annotation:

```c++
/**
 * Case description: Test that when the cond input of the If operator is a scalar with value 1, execution enters the then branch
 * Preconditions:
 *   1. Fake Shape and Rank operator's lowering and execution kernel
 * Test steps:
 *   1. Construct an If graph, the then branch contains a Shape node, the else branch contains a Rank node
 *   2. Lower and load this If graph
 *   3. Attach ESS to monitor the execution graph's execution status
 *   4. Construct two Data, one is scalar value 1 passed as cond; another is Tensor with shape {1,1}
 *   5. Execute
 * Expected results:
 *   1. Network output result is {2}
 *   2. Through ESS observation, Rank was not executed, Shape was executed
 */
```

## ST Case Scenario Design Checklist

| No. | Scenario | Description |
| ---- | ------------------------------------------ | ------------------------------------------------------------ |
| 1    | Empty/scalar Tensor                           | Theoretically every feature faces the impact of empty tensors. Empty tensors include both output empty tensor and input empty tensor scenarios. Most empty tensor coverage testing should be completed at the UT stage. However, UT is single-point testing and cannot represent the entire feature's support for empty tensors. Therefore, to be safe, if related to empty tensors, adding an ST case is worthwhile. |
| 2    | Maintainability (Profiling, DataDump, ExceptionDump) | If a feature makes changes to the execution graph, it may affect maintainability features (maintainability features are based on certain characteristics of the execution graph). We are currently designing how to make new feature developers test maintainability features automatically without awareness, but before this testing capability goes live, new feature developers still need to evaluate whether their new features affect maintainability. If uncertain, you can simply add a corresponding case. |

## How to Validate

### Graph Compilation Validation

The graph compilation process is divided into graph preparation, graph optimization, and graph compilation from a process perspective. The first two stages mainly modify the graph, and the last stage completes operations such as stream allocation, task generation, and memory allocation (for static models). Certain key attributes are also saved on the graph.

#### Graph Validation

During the graph optimization process, graphs are dumped stage by stage. You can validate nodes, edge relationships, attributes, etc. on the graph by dumping a certain stage. Use DUMP_GRAPH_WHEN and CHECK_GRAPH macros together. The first macro specifies which stage's graph to dump, and the second macro can write graph validation logic.
```c++
/**
 * 
 *       Variable(2, 3, 4, 5)                      Relu3(1,2,3,4,5)
 *          /            \                             /        \
 *     TransData      TransData                    TransData    summary
 *         |              |                            |
 *   Relu(1,2,3,4,5)  Relu(1,2,3,4,5)            Variable(2, 3, 4, 5)
 *          \            /                             |
 *             NetOutput -------------------------------
 * 
 * Case description: After Summary optimization, Summary operator is deleted, its input node connects to Netoutput as network output
 * Preconditions:
 * 1. Register Summary operator's graph
 * Test steps:
 * 1. Construct a graph with Summary operator
 * 2. Specify DUMP PreRunAfterHandleSummaryOp this graph
 * 3. Construct Session, execute AddGraph and RunGraph interfaces, execute graph compilation operation
 * 4. Validate against PreRunAfterHandleSummaryOp graph
 * Expected results:
 * 1. On PreRunAfterHandleSummaryOp graph, Summary operator is deleted
 * 2. Netoutput's outputs increase from 2 to 3. Relu3 connects to Netoutput as output
 * 3. SummaryCallback function was called once
*/
TEST_F(SummaryAndCheckPointTest, test_optimize_summary_graph) {
  // build graph
  Graph graph = BuildVariableGraph();

  // new session & add graph
  map<AscendString, AscendString> options;
  options[ge::OPTION_GRAPH_RUN_MODE] = AscendString("1"); // train flag
  options[VARIABLE_MEMORY_MAX_SIZE] = "12800";
  Session session(options);
  uint32_t graph_id = 1;
  auto ret = session.AddGraph(graph_id, graph, options);
  EXPECT_EQ(ret, SUCCESS);

  // register Summary call back func
  static uint32_t called_count = 0;
  ge::session::pCallBackFunc callbackFuncSummary = [](uint32_t graph_id, const std::map<AscendString, ge::Tensor> &params_list) {
    called_count++;
    return SUCCESS;
  };
  ret = session.RegisterCallBackFunc("Summary", callbackFuncSummary);
  EXPECT_EQ(ret, SUCCESS);

  // build input tensor
  std::vector<Tensor> inputs;
  Shape shape({1, 1, 224, 224});
  TensorDesc tensor_desc(shape, FORMAT_NCHW, DT_FLOAT);
  std::vector<uint8_t> data(224 * 224 * sizeof(float));
  Tensor tensor(tensor_desc, data);
  inputs.emplace_back(tensor);

  std::vector<Tensor> outputs;
  // build_graph through session
  DUMP_GRAPH_WHEN("PreRunAfterHandleSummaryOp");
  ret = session.RunGraph(graph_id, inputs, outputs);
  EXPECT_EQ(ret, SUCCESS);
  // check result
  CHECK_GRAPH(PreRunAfterHandleSummaryOp) {
    auto summary = graph->FindNode("summary");
    ASSERT_EQ(summary, nullptr); // summary has been deleted
    auto netoutput = graph->FindNode("output");
    ASSERT_NE(netoutput, nullptr);
    ASSERT_EQ(netoutput->GetInDataNodes().size(), 4); // add relu3 as input of netoutput
    ASSERT_EQ(netoutput->GetInDataNodes().at(3)->GetName(), "relu3");
  };
  EXPECT_EQ(called_count, 1);
}
```

#### DUMP_GRAPH_WHEN Available Phase List

The following are `DUMP_GRAPH_WHEN` phase names actually used in existing cases. When writing ST cases, you can select appropriate phases to capture graph snapshots as needed:

| Category | Phase Name | Description |
|------|---------|------|
| **Graph Preparation** | `PrepareAfterUpdateInputOutputByUserOptions` | After user options update input/output |
| | `PrepareAfterInsertAipp` | After AIPP insertion |
| | `PrepareAfterProcessAippStage2` | After AIPP processing stage 2 |
| | `PrepareAfterCheckAndUpdateInput` | After checking and updating input |
| | `PrepareAfterInferFormatAndShape` | After inferring format and shape |
| | `PrepareAfterPrepareOptimize` | After preparation optimization |
| **Graph Optimization** | `PreRunBegin` | PreRun start |
| | `PreRunAfterNormalizeGraph` | After graph normalization |
| | `PreRunAfterHandleSummaryOp` | After Summary operator processing |
| | `PreRunAfterOptimizeSubgraph` | After subgraph optimization |
| | `PreRunAfterOptimize1` | After first round of optimization |
| | `PreRunAfterOptimize2` | After second round of optimization |
| | `PreRunAfterPrepareRunningFormatRefiner` | After runtime format refinement |
| | `PreRunAfterOptimizeGraphBeforeBuild` | After pre-build graph optimization |
| | `PreRunAfterInitPreparation` | After initialization preparation |
| | `PreRunAfterMemConflictProc` | After memory conflict processing |
| | `PreRunAfterPrepare` | After preparation completion |
| | `PreRunAfterBuild` | After build completion |
| | `OptimizeStage1_1` / `OptimizeStage1_2` | Optimization stage 1 |
| | `OptimizeOriginalGraph_FeGraphFusionAfter` | After FE graph fusion |
| | `OptimizeOriginalGraph_FeInsertTransNodeAfter` | After FE TransNode insertion |
| | `OptimizeOriginalGraph_FeDistHeavyFormatAfter` | After FE distributed heavy format |
| | `OptimizeQuantGraph_FeGraphFusionAfter` | After quantization graph FE fusion |
| | `OptimizeGraph_TagNoConstFoldingAfter` | After marking no constant folding |
| | `BeforeOptimizeOriginalGraphJudgeInsert` | Before original graph optimization judgment insertion |
| | `AfterRecompute` | After recomputation |
| | `AfterDynamicShapePartition` | After dynamic shape partition |
| | `After_AutoFusePass` | After auto fusion pass |
| | `AutoFuser_BeforeAutoFuse` | Before auto fusion |
| **Graph Build** | `Build` | Build phase |
| | `before_build` / `after_build` | Before/after build |
| | `AfterAssignResource` | After resource allocation |
| | `AfterGetTask` | After getting task |
| | `AfterCalcOpParam` | After operator parameter calculation |
| | `AfterInfershape` | After shape inference |
| | `AfterSwapSpace` / `BeforeSwapSpace` | SwapSpace processing |
| | `MergedComputeGraphAfterCompositeEnginePartition` | After composite engine partition merge |

> This list is derived from actual usage in existing cases and may be incomplete. If you need to dump new phases, you can register corresponding dump phase names in production code.

### Graph Execution Validation

For graph execution, you need to construct a corresponding network and produce different results based on different inputs. ST cases directly validate the network's output results.
```c++
void RunIfGraph2(TensorHolder &pred_tensor, bool expect_branch) {
    // Construct an If graph
    auto compute_graph = ShareGraph::IfGraph2();
    ASSERT_NE(compute_graph, nullptr);
    compute_graph->TopologicalSorting();
    GeModelBuilder builder(compute_graph);
    auto ge_root_model = builder.BuildGeRootModel();

    // Stub Shape/Rank operators, take over execution of these two operators
    GertRuntimeStub rtstub;
    rtstub.StubByNodeTypes({"Shape", "Rank"});

    // lowering
    auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
    ASSERT_NE(exe_graph, nullptr);
    auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
    ASSERT_NE(model_executor, nullptr);

    // Create an observer to observe the execution graph's execution status
    auto ess = StartExecutorStatistician(model_executor);

    // Load
    EXPECT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

    // Construct network input stream and tensor
    auto input_holder = TensorFaker().Placement(kOnDeviceHbm).DataType(ge::DT_FLOAT).Shape({8, 3, 224, 224}).Build();
    std::vector<Tensor *> inputs{pred_tensor.GetTensor(), input_holder.GetTensor()};
    auto output_holder = TensorFaker().Placement(kOnHost).DataType(ge::DT_INT64).Shape({8}).Build();
    std::vector<Tensor *> outputs{output_holder.GetTensor()};
    rtStream_t stream;
    ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
    auto stream_value = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

    // Expected network output value, because this is a general test function,
    // the expected true/false branch determines whether to execute shape or rank operator,
    // thus having different expected output values
    auto shape_count = expect_branch ? 1 : 0;
    auto rank_count = expect_branch ? 0 : 1;
    auto output_shape = expect_branch ? Shape({4}) : Shape({});
    auto output_data = expect_branch ? Shape({8, 3, 224, 224}) : Shape({4});

    // Execute
    ess->Clear();
    ASSERT_EQ(
        model_executor->Execute({stream_value.value}, inputs.data(), inputs.size(), outputs.data(), outputs.size()),
        ge::GRAPH_SUCCESS);
    // Validate Shape/Rank node execution count meets expectations
    EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("Shape", "FakeExecuteNode"), shape_count);
    EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("Rank", "FakeExecuteNode"), rank_count);
    EXPECT_EQ(output_holder.GetTensor()->GetShape().GetStorageShape(), output_shape);
    EXPECT_EQ(output_holder.GetTensor()->GetShape().GetOriginShape(), output_shape);
    EXPECT_EQ(memcmp(output_holder.GetTensor()->GetAddr(), &output_data[0], output_data.GetDimNum() * 8), 0);

    // Cleanup resources
    ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
    rtStreamDestroy(stream);
  }
```

### Two-Round Execution Validation

Sometimes a business distinguishes between initialization and running states, for example:

* Graph execution distinguishes between graph loading and graph execution phases. First execution does loading and execution, subsequent executions only do execution
* For end-to-end execution of a computational graph, first execution goes through compilation, loading, and execution phases. Second execution skips compilation and loading, only doing execution

For such businesses, consider making two rounds of calls to the running state and validating the results of both rounds separately.

The reason for doing two rounds of validation is that for such businesses, during the first round of execution, the system is in an initialized state. After the first round, there may be dirty data in the system that affects the second round's execution results. Making two rounds of calls can effectively validate dirty data issues.
```c++
  // First execution
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    outputs.data(), outputs.size()),
            ge::GRAPH_SUCCESS);
  // More validation details omitted here

  // Second execution
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    outputs.data(), outputs.size()),
            ge::GRAPH_SUCCESS);
  // Same detailed validation as after first execution

  // Model unload and destroy
  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
```