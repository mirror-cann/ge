# ST用例开发指导

> 返回 [DT用例开发总纲](README.md)

ST的测试范围是一个特性，从大模块角度（例如编译时、执行时）使能和测试某个特性，并观察编译/执行的结果是否符合预期。ST用例与STC对应，因此相对于UT用例，ST用例更正式一些，对应地数量也更少一些。

## ST用例的注释要求

每个ST都需要对应一个STC设计，ST的注释便是STC对用例的描述。在需求设计阶段，STC通过excel等方式承载，编码完成后，STC的设计通过ST用例的注释来承载，后续变更、新增ST用例均同步修改对应注释，STC的原始excel文件不再维护。ST用例的注释中，需要包含四部分内容：

* 用例描述：一句描述本用例的测试场景
* 预置条件：如何构造ST环境，fake、打桩哪些基础环境数据
* 测试步骤：简要描述测试的步骤
* 预期结果：期望得到什么样的结果，如果有多条，那么分条编写

举例说明，比如要测试If节点在rt2的执行是否正常，可能有如下的ST用例注释：

```c++
/**
 * 用例描述：测试If算子中，当cond输入为一个scalar，并且为1时，进入then分支执行
 * 预置条件：
 *   1. Fake Shape和Rank算子的lowering和执行kernel
 * 测试步骤：
 *   1. 构造一张If图，then分支中包含一个Shape节点，else分支包含一个Rank节点
 *   2. lowering、加载这张If图
 *   3. 挂接ESS，监控执行图的执行情况
 *   4. 构造两个Data，一个为scalar值1，传递为cond；另一个为Tensor，shape是{1,1}
 *   5. 执行
 * 预期结果：
 *   1. 网络输出结果是{2}
 *   2. 通过ess观察可知，Rank未被执行，Shape被执行
 */
```

## ST用例场景设计checklist

| 序号 | 场景                                       | 说明                                                         |
| ---- | ------------------------------------------ | ------------------------------------------------------------ |
| 1    | 空/scalar Tensor                           | 理论上每个特性都会面临空Tensor的影响，空Tensor包含输出空Tensor和输入空Tensor两种场景。对空Tensor的覆盖测试，大部分应该在UT阶段完成。但是UT为单点测试，并不能代表整个特性对空Tensor的支持度。因此稳妥起见，若与空tensor相关，ST补充一个用例是值得的。 |
| 2    | 可维（Profiling、DataDump、ExceptionDump） | 如果某个特性对执行图做了变更，那么可能会影响到可维特性（可维特性是基于执行图的某些特征做的）。当前我们正在设计如何让新特性的开发者无感知地自动测试可维特性，但是在这个测试能力上线前，新特性的开发者仍然需要评估自己新增的特性是否影响了可维。如果不确定，可以简单地增加一个对应用例。 |

## 如何校验

### 图编译校验

图编译过程从流程上来看分为图准备、图优化、图编译。前两个阶段主要对图做修改，最后一个阶段完成流分配、生成task、内存分配（若为静态模型）等操作，某些关键属性也会保存在图上。

#### 图的校验

图优化的过程中会阶段性地dump图，可以通过dump某个阶段图对图上节点、连边关系、属性等校验。配对使用DUMP_GRAPH_WHEN、CHECK_GRAPH这两个宏。第一个宏指定了dump哪个阶段的图，第二个宏里可以写图校验逻辑。
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
 * 用例描述:经过Summary优化，Summary算子被删除，其输入节点作为整网输出连接到Netoutput上
 * 预置条件：
 * 1. 注册Summary算子的图
 * 测试步骤：
 * 1. 构造一张带有Summary算子的图
 * 2. 指定DUMP PreRunAfterHandleSummaryOp 这张图
 * 3. 构造Session，执行AddGraph和RunGraph接口，执行图编译操作
 * 4. 针对 PreRunAfterHandleSummaryOp 这张图做校验
 * 预期结果：
 * 1. PreRunAfterHandleSummaryOp这张图上，Summary算子被删除
 * 2. Netoutput的输出由2个变成了3个。Relu3作为输出连到了Netoutput上
 * 3. SummaryCallback函数被调用了1次
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

#### DUMP_GRAPH_WHEN 可用阶段列表

以下是在现有用例中实际使用的 `DUMP_GRAPH_WHEN` 阶段名称，编写ST用例时可根据需要选择合适的阶段捕获图快照：

| 分类 | 阶段名称 | 说明 |
|------|---------|------|
| **图准备** | `PrepareAfterUpdateInputOutputByUserOptions` | 用户选项更新输入输出后 |
| | `PrepareAfterInsertAipp` | AIPP插入后 |
| | `PrepareAfterProcessAippStage2` | AIPP处理第二阶段后 |
| | `PrepareAfterCheckAndUpdateInput` | 检查并更新输入后 |
| | `PrepareAfterInferFormatAndShape` | 推断格式和Shape后 |
| | `PrepareAfterPrepareOptimize` | 准备优化后 |
| **图优化** | `PreRunBegin` | PreRun开始 |
| | `PreRunAfterNormalizeGraph` | 图规范化后 |
| | `PreRunAfterHandleSummaryOp` | Summary算子处理后 |
| | `PreRunAfterOptimizeSubgraph` | 子图优化后 |
| | `PreRunAfterOptimize1` | 第一轮优化后 |
| | `PreRunAfterOptimize2` | 第二轮优化后 |
| | `PreRunAfterPrepareRunningFormatRefiner` | 运行时格式精炼后 |
| | `PreRunAfterOptimizeGraphBeforeBuild` | 构建前图优化后 |
| | `PreRunAfterInitPreparation` | 初始化准备后 |
| | `PreRunAfterMemConflictProc` | 内存冲突处理后 |
| | `PreRunAfterPrepare` | 准备完成后 |
| | `PreRunAfterBuild` | 构建完成后 |
| | `OptimizeStage1_1` / `OptimizeStage1_2` | 优化阶段1 |
| | `OptimizeOriginalGraph_FeGraphFusionAfter` | FE图融合后 |
| | `OptimizeOriginalGraph_FeInsertTransNodeAfter` | FE插入TransNode后 |
| | `OptimizeOriginalGraph_FeDistHeavyFormatAfter` | FE分布式重格式后 |
| | `OptimizeQuantGraph_FeGraphFusionAfter` | 量化图FE融合后 |
| | `OptimizeGraph_TagNoConstFoldingAfter` | 标记无常量折叠后 |
| | `BeforeOptimizeOriginalGraphJudgeInsert` | 原始图优化判断插入前 |
| | `AfterRecompute` | 重计算后 |
| | `AfterDynamicShapePartition` | 动态Shape分区后 |
| | `After_AutoFusePass` | 自动融合Pass后 |
| | `AutoFuser_BeforeAutoFuse` | 自动融合前 |
| **图构建** | `Build` | 构建阶段 |
| | `before_build` / `after_build` | 构建前/后 |
| | `AfterAssignResource` | 资源分配后 |
| | `AfterGetTask` | 获取Task后 |
| | `AfterCalcOpParam` | 算子参数计算后 |
| | `AfterInfershape` | 推断Shape后 |
| | `AfterSwapSpace` / `BeforeSwapSpace` | SwapSpace处理 |
| | `MergedComputeGraphAfterCompositeEnginePartition` | 复合引擎分区合并后 |

> 该列表来源于现有用例的实际使用，可能不完整。如果需要 dump 新的阶段，可以在生产代码中通过注册对应的 dump 阶段名来实现。

### 图执行校验

对于图执行，需要构造对应网络，根据不同的输入，产生不同的结果，ST用例直接校验网络的输出结果。
```c++
void RunIfGraph2(TensorHolder &pred_tensor, bool expect_branch) {
    // 构造一张If图
    auto compute_graph = ShareGraph::IfGraph2();
    ASSERT_NE(compute_graph, nullptr);
    compute_graph->TopologicalSorting();
    GeModelBuilder builder(compute_graph);
    auto ge_root_model = builder.BuildGeRootModel();

    // 对Shape/Rank算子打桩，接管这两个算子的执行
    GertRuntimeStub rtstub;
    rtstub.StubByNodeTypes({"Shape", "Rank"});

    // lowering
    auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
    ASSERT_NE(exe_graph, nullptr);
    auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
    ASSERT_NE(model_executor, nullptr);

    // 创建一个观察者，观察执行图的执行状况
    auto ess = StartExecutorStatistician(model_executor);

    // 加载
    EXPECT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

    // 构造网络的输入stream和tensor
    auto input_holder = TensorFaker().Placement(kOnDeviceHbm).DataType(ge::DT_FLOAT).Shape({8, 3, 224, 224}).Build();
    std::vector<Tensor *> inputs{pred_tensor.GetTensor(), input_holder.GetTensor()};
    auto output_holder = TensorFaker().Placement(kOnHost).DataType(ge::DT_INT64).Shape({8}).Build();
    std::vector<Tensor *> outputs{output_holder.GetTensor()};
    rtStream_t stream;
    ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
    auto stream_value = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

    // 期望的网络输出值，因为这是个通用的测试函数，因此根据期望的true/false分支，而走shape或rank算子，进而会有不同的期望输出值
    auto shape_count = expect_branch ? 1 : 0;
    auto rank_count = expect_branch ? 0 : 1;
    auto output_shape = expect_branch ? Shape({4}) : Shape({});
    auto output_data = expect_branch ? Shape({8, 3, 224, 224}) : Shape({4});

    // 执行
    ess->Clear();
    ASSERT_EQ(
        model_executor->Execute({stream_value.value}, inputs.data(), inputs.size(), outputs.data(), outputs.size()),
        ge::GRAPH_SUCCESS);
    // 校验Shape/Rank节点的执行次数是否符合预期
    EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("Shape", "FakeExecuteNode"), shape_count);
    EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("Rank", "FakeExecuteNode"), rank_count);
    EXPECT_EQ(output_holder.GetTensor()->GetShape().GetStorageShape(), output_shape);
    EXPECT_EQ(output_holder.GetTensor()->GetShape().GetOriginShape(), output_shape);
    EXPECT_EQ(memcmp(output_holder.GetTensor()->GetAddr(), &output_data[0], output_data.GetDimNum() * 8), 0);

    // 清理资源
    ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
    rtStreamDestroy(stream);
  }
```

### 两轮运行校验

有时一个业务会区分初始化和运行两种状态，例如：

* 图执行会区分图加载和图执行两个阶段，首次执行做加载、执行，二次及以后仅做执行
* 对于端到端的执行一张计算图，首次执行会经历编译、加载、执行三个阶段，二次执行会跳过编译、加载，仅做执行

对于此类业务，考虑对运行状态做两轮调用，对两轮调用的结果分别校验。

做两轮校验的原因是，对于此类业务，首轮运行时，系统处于初始化状态，首轮运行后，系统中可能存在脏数据，影响二轮执行的结果。做两轮调用可以有效校验脏数据问题。
```c++
  // 第一次执行
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    outputs.data(), outputs.size()),
            ge::GRAPH_SUCCESS);
  // 更多校验细节，这里省略

  // 第二次执行
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    outputs.data(), outputs.size()),
            ge::GRAPH_SUCCESS);
  // 与第一次执行结束后相同的细节校验

  // 模型卸载和销毁
  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
```
