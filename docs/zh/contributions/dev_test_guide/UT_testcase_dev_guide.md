# UT用例开发指导

> 返回 [DT用例开发总纲](README.md)

UT的测试范围是一个文件，理论上一个UT应该对一个文件暴露的接口做测试，并校验属于被测文件的行为。测试过程中，UT用例应该认为**其他文件的行为都是正确的、仅校验被测文件的行为**。

## UT用例设计

设计一个接口的测试点时，注意关注以下点，以下点为checklist，如果不涉及，或者认为不需要校验，可以不设计对应用例：

* 错误值校验，例如
  * 入参空指针
  * 超出规格范围

* 边界值校验，例如
  * 如果输入涉及Tensor，考虑空Tensor场景
* 如果被测模块涉及容器概念（例如可以添加或管理多个element），那么需要额外考虑如下因素
  * 插入动作发生在容器的开头、中间、结尾时
  * 删除动作发生在容器的开头、中间、结尾时

## 如何校验

### 做完整校验

UT用例应该根据设计，对所调用函数的返回值、出参做完整校验。如果所调用的函数产生了副作用，那么应该对副作用做完整校验。所谓"完整校验"是指从外部可以观察到的、所有的变更点，都应该校验。

一种典型的错误是仅校验被测函数的返回值，这种用例的作用极其有限，应该避免。

### 避免重复校验

用例应该避免重复校验，这意味着同一类校验尽量不要出现在多个不同的用例中。重复校验的主要问题在于加大了用例的维护难度，在修改一个模块行为时，导致多个用例失效。

### 图类UT关注的校验点

改图、构图类UT，需要关注图的变化，例如：

* 新增结点检查
* 删除结点检查
* 变更的连边检查
* 变更的属性检查
以CEM的基础UT为例，CEM(Constant Expression Motion)作用于执行图，将执行图Main图中每次计算中不变的表达式移动到Init图中，以达到加速Main图执行速度的作用。下面的用例中，构造了一个注释中的Main Graph，并期望通过一个Pass，将Main图中的两个Const，一个Foo1移动到Init图中：

```c++
/*
 * main graph:
 *
 *          NetOutput
 *             |
 *           Foo2
 *         /   \
 *      Foo1    data
 *     /   \
 * const   const
 */
TEST_F(ConstantExpressionsMotionUT, MoveToInit_Success_CeOnMain) {
  auto c1 = ValueHolder::CreateConst("Hello", 5, true);
  auto c2 = ValueHolder::CreateConst("World", 5, true);
  auto data = ValueHolder::CreateFeed(0);
  auto foo1 = ValueHolder::CreateSingleDataOutput("Foo", {c1, c2});
  auto foo2 = ValueHolder::CreateSingleDataOutput("Foo", {foo1, data});
  auto main_frame = ValueHolder::PopGraphFrame({foo2}, {}, "NetOutput");
  auto root_frame = ValueHolder::PopGraphFrame();
  ASSERT_NE(root_frame, nullptr);
  ASSERT_NE(main_frame, nullptr);

  bool changed = false;
  LoweringGlobalData global_data;
  ASSERT_EQ(ConstantExpressionMotion(&global_data).Run(root_frame->GetExecuteGraph().get(), changed),
            ge::GRAPH_SUCCESS);
  ASSERT_TRUE(changed);

  EXPECT_EQ(ExeGraphSummaryChecker(main_frame->GetExecuteGraph().get())
                .StrictDirectNodeTypes(
                    std::map<std::string, size_t>{{"Data", 1}, {"Foo", 1}, {"InnerData", 1}, {"NetOutput", 1}}),
            "success");

  EXPECT_EQ(ExeGraphSummaryChecker(init_frame_->GetExecuteGraph().get())
                .StrictDirectNodeTypes(std::map<std::string, size_t>{{"Const", 2}, {"Foo", 1}, {"InnerNetOutput", 1}}),
            "success");
  EXPECT_EQ(de_init_frame_->GetExecuteGraph()->GetDirectNodesSize(), 0);

  ConnectFromInitToMain(foo1->GetFastNode(), 0, foo2->GetFastNode(), 0);

  changed = false;
  ASSERT_EQ(ConstantExpressionMotion(&global_data).Run(root_frame->GetExecuteGraph().get(), changed),
            ge::GRAPH_SUCCESS);
  ASSERT_FALSE(changed);
}
```

### rt2 kernel类UT的校验点

#### Kernel函数的范围

RT2中的Kernel指为执行节点注册的一系列运行时函数的集合，当前包括：
- 通过.RunFunc注册的Kernel执行函数，即执行节点的计算逻辑
- 通过.OutputsCreatorFunc注册的执行节点输出创建及初始化函数
> 注意，早期版本中的.OutputsInitializer及.OutputsCreator注册接口已废弃。
- 通过.TracePrinter注册的节点自定义执行信息组装接口
> 该接口在节点执行结束后调用，返回该节点类型关注的context信息，供框架进行节点执行trace信息打印。

Kernel函数只为执行节点服务，其函数签名不具备可读性，通常应当设置为匿名函数。

#### Kernel函数UT测试的策略

对Kernel的UT测试是对某个执行节点的运行时相关函数的测试，应当从执行节点发起，而不是直接调Kernel函数。
> 有些用例为了可以在UT中调用而将不可读的kernel函数签名符号导出，有些本末倒置。

框架保证调用Kernel相关函数时，已正确申请输入输出any value内存，Kernel函数编码时，可以对输入输出的any value进行ASSERT防御性校验，但是不推荐对其进行单独的UT测试：即不要测试某个输入any value为空时的Kernel表现。
> 这类用例当前代码里通常是一个叫"输入异常"的笼统名称，预期执行失败。但是都是经不起推敲的，比如为何测试只测试一个异常输入场景，而不测试不同异常组合下的返回？名言"部分的证明等于0"说明了这种验证的意义。

#### Kernel函数的测试点说明

> 你可能需要关注测试点4中对TracePrinter函数的相关说明

##### 测试点1：根据执行节点类型能正确查询到注册的Kernel系列函数
```c++
  auto funcs = registry.FindKernelFuncs("YourTestingOpType");
  ASSERT_NE(funcs, nullptr);
  ASSERT_NE(funcs->outputs_creator, nullptr); // 根据是否注册决定是否校验
  ASSERT_EQ(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS); // 根据是否注册决定是否校验
  ASSERT_NE(context->GetOutputPointer<Shape>(0), nullptr); // 根据是否注册决定是否校验
```
##### 测试点2：对执行节点的OutputsCreatorFunc函数测试

如果执行节点注册了OutputsCreatorFunc，则对OutputsCreatorFunc函数进行UT测试，通常只需要测试OutputsCreatorFunc的返回值正确即可。
```c++
  ASSERT_NE(funcs->outputs_creator, nullptr);
  ASSERT_EQ(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS);
```
我们建议只写一个校验OutputsCreatorFunc正常执行的用例出于以下考虑：
- 接口返回正确代表完成了全部的创建和初始化动作，对创建的内容的校验是冗余的
- 对创建内容的校验是不充分且不可信的，因为无法从any value对象存储的指针上校验类型

#####  测试点3：对执行节点的Kernel执行函数测试

这部分需要针对Kernel执行函数的实现设计合理的测试用例。
但是需要明确用例的边界：在调用Kernel执行函数前，需要保证已经正确创建了KernelContext输入输出any value内存，并调用OutputsCreatorFunc注册函数（如果有的话），这两部分是测试的前置条件。
```c++
TEST_F(BuildTensorUT, SplitTensor_Host) {
  auto tensor_holder = TensorFaker().Shape({10, 20}).Format(ge::FORMAT_ND).Placement(kOnHost).Build();
  auto context_holder = KernelRunContextFaker()
                            .KernelIONum(2, static_cast<size_t>(kernel::SplitTensorOutputs::kNum))
                            .NodeIoNum(1, 1)
                            .Inputs({tensor_holder.GetTensor(), &gert_allocator})
                            .Build();

  auto run_context = context_holder.GetContext<KernelContext>();
  auto funcs = registry.FindKernelFuncs(kernel::kSplitDataTensor);
  ASSERT_NE(funcs, nullptr);
  ASSERT_EQ(funcs->outputs_creator(FastNodeFaker().Build(), run_context), ge::GRAPH_SUCCESS);
  ASSERT_EQ(funcs->run_func(run_context), ge::GRAPH_SUCCESS); // 调用执行函数

  // check tensor data
  auto tensor_data_chain = run_context->GetOutput(static_cast<size_t>(kernel::SplitTensorOutputs::kTensorData)); // 获取输出并校验结果
  ASSERT_NE(tensor_data_chain, nullptr);
  EXPECT_TRUE(tensor_data_chain->HasDeleter());
  auto tensor_data = tensor_data_chain->GetPointer<GertTensorData>();
  ASSERT_NE(tensor_data, nullptr);
  EXPECT_EQ(tensor_data->GetAddr(), tensor_holder.GetTensor()->GetAddr());

  // check shape
  auto shape = run_context->GetOutputPointer<StorageShape>(static_cast<size_t>(kernel::SplitTensorOutputs::kShape));
  ASSERT_NE(shape, nullptr);
  EXPECT_EQ(*shape, tensor_holder.GetTensor()->GetShape());

  context_holder.FreeAll();
}

```
> 注意，如果你正在写一个期望Kernel执行函数失败的用例，请务必慎重：这是否是一个真实存在的运行时输入场景？如果不是，很容易陷入"部分证明"的泥潭 -- 为何不测试全部的异常输入组合。即使你罗列了全部的异常组合，也会陷入用例数量爆炸的另一个泥潭。

我们可以在Kernel执行函数内部，进行一些BUG场景的ASSERT防御性校验，但是不应该对其进行单独的UT测试。

#####  测试点4：对TracePrinter函数进行测试

TracePrinter函数的功能，是从context中收集所需的执行信息，在调用时序上发生在Kernel执行函数调用后。

> 需要特别注意的是，当前执行逻辑下，即使Kernel执行函数返回错误，仍会调用该函数，因此函数实现时对于Kernel输出的处理需要格外慎重，必须与Kernel执行函数严格匹配。在目前调用时机未修改且无法获知执行结果的情况下，需要开发者谨慎地实现TracePrinter函数，确保在Kernel执行函数异常退出时，不会发生运行时错误，即使这很难实现。将来大概率会修改该函数的调用时机，或调用时传入执行结果信息。

基于当前的调用时机，我们只建议对TracePrinter进行两部分的测试：
- 测试用例1：测试Kernel执行函数正常执行后的返回信息
```c++
  registry.FindKernelFuncs("LaunchKernelWithFlag")->run_func(context.WithFlag())
  auto ret = registry.FindKernelFuncs("LaunchKernelWithFlag")->trace_printer(context.WithFlag());
  EXPECT_FALSE(ret.empty());
```
> 注意，该测试是不充分的，假定了处理执行函数错误时，不对输出进行任何修改，这通常是不成立的。

- 测试用例2：模拟测试Kernel执行函数异常时的返回信息（待定）
```
构造 ->run_func() 失败的场景，然后调用 ->trace_printer 方法
```
