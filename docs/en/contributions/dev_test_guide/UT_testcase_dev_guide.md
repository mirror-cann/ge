# UT Case Development Guide

> Return to [DT Test Case Development Guide](README.md)

UT's testing scope is a file. Theoretically, a UT should test the interfaces exposed by a file and validate behaviors belonging to the tested file. During testing, UT cases should assume **other files' behaviors are correct and only validate the tested file's behavior**.

## UT Case Design

When designing test points for an interface, pay attention to the following points. These points form a checklist. If not applicable or considered unnecessary to validate, you can skip designing corresponding cases:

* Error value validation, for example
  * Input parameter null pointer
  * Out of specification range

* Boundary value validation, for example
  * If input involves Tensor, consider empty Tensor scenarios
* If the tested module involves container concepts (e.g., can add or manage multiple elements), then need to additionally consider the following factors
  * Insertion occurs at the beginning, middle, or end of the container
  * Deletion occurs at the beginning, middle, or end of the container

## How to Validate

### Do Complete Validation

UT cases should perform complete validation of the called function's return values and output parameters according to design. If the called function produces side effects, then complete validation of side effects should be done. "Complete validation" means all observable change points from the outside should be validated.

A typical mistake is only validating the tested function's return value. Such cases have extremely limited value and should be avoided.

### Avoid Repeated Validation

Cases should avoid repeated validation, which means the same type of validation should not appear in multiple different cases. The main problem with repeated validation is that it increases case maintenance difficulty. When modifying a module's behavior, it causes multiple cases to fail.

### Graph Class UT Validation Points

Graph modification and graph construction class UTs need to pay attention to graph changes, for example:

* New node check
* Deleted node check
* Changed edge check
* Changed attribute check

Taking CEM's basic UT as an example, CEM (Constant Expression Motion) acts on execution graphs, moving invariant expressions in each computation in the Main graph to the Init graph to accelerate Main graph execution speed. In the following case, a Main Graph is constructed as shown in the comments, and it's expected that through a Pass, two Consts and one Foo1 in the Main graph will be moved to the Init graph:

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

### rt2 Kernel Class UT Validation Points

#### Kernel Function Scope

Kernel in RT2 refers to a collection of runtime functions registered for execution nodes, currently including:
- Kernel execution functions registered through .RunFunc, i.e., the computation logic of execution nodes
- Execution node output creation and initialization functions registered through .OutputsCreatorFunc
> Note: The .OutputsInitializer and .OutputsCreator registration interfaces in early versions have been deprecated.
- Node custom execution information assembly interfaces registered through .TracePrinter
> This interface is called after node execution ends, returning context information the node type cares about, for framework node execution trace information printing.

Kernel functions only serve execution nodes. Their function signatures are not readable and should typically be set as anonymous functions.

#### Kernel Function UT Testing Strategy

UT testing of Kernel is testing of runtime-related functions of a certain execution node, which should start from the execution node, not directly calling Kernel functions.
> Some cases export unreadable kernel function signatures for calling in UT, which puts the cart before the horse.

The framework guarantees that when calling Kernel-related functions, input/output any value memory has been correctly allocated. When coding Kernel functions, you can perform ASSERT defensive validation on input/output any values, but it's not recommended to do separate UT tests on them: don't test Kernel behavior when a certain input any value is empty.
> Such cases in current code are usually named "input exception" with expected execution failure. But they don't stand up to scrutiny, like why only test one exception input scenario and not test returns under different exception combinations? The famous saying "partial proof equals zero" illustrates the meaninglessness of such validation.

#### Kernel Function Test Point Description

> You may need to pay attention to the related description of TracePrinter function in test point 4

##### Test Point 1: Can correctly query registered Kernel series functions according to execution node type
```c++
  auto funcs = registry.FindKernelFuncs("YourTestingOpType");
  ASSERT_NE(funcs, nullptr);
  ASSERT_NE(funcs->outputs_creator, nullptr); // Validate based on whether registered
  ASSERT_EQ(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS); // Validate based on whether registered
  ASSERT_NE(context->GetOutputPointer<Shape>(0), nullptr); // Validate based on whether registered
```
##### Test Point 2: Test execution node's OutputsCreatorFunc function

If the execution node registered OutputsCreatorFunc, then UT test the OutputsCreatorFunc function. Usually only need to test that OutputsCreatorFunc's return value is correct.
```c++
  ASSERT_NE(funcs->outputs_creator, nullptr);
  ASSERT_EQ(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS);
```
We suggest only writing one case to validate OutputsCreatorFunc normal execution based on the following considerations:
- Correct interface return means all creation and initialization actions are complete, validation of created content is redundant
- Validation of created content is insufficient and untrustworthy, because type cannot be validated from pointers stored in any value objects

#####  Test Point 3: Test execution node's Kernel execution function

This part needs to design reasonable test cases according to Kernel execution function implementation.
But need to clarify case boundaries: Before calling Kernel execution function, need to ensure KernelContext input/output any value memory has been correctly created and OutputsCreatorFunc registration function called (if any). These two parts are test preconditions.
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
  ASSERT_EQ(funcs->run_func(run_context), ge::GRAPH_SUCCESS); // Call execution function

  // check tensor data
  auto tensor_data_chain = run_context->GetOutput(static_cast<size_t>(kernel::SplitTensorOutputs::kTensorData)); // Get output and validate result
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
> Note: If you're writing a case expecting Kernel execution function to fail, please be cautious: Is this a real runtime input scenario? If not, it's easy to fall into the "partial proof" quagmire -- why not test all exception input combinations. Even if you list all exception combinations, you'll fall into another quagmire of case count explosion.

We can do some BUG scenario ASSERT defensive validation inside Kernel execution function, but shouldn't do separate UT tests on it.

#####  Test Point 4: Test TracePrinter function

TracePrinter function's functionality is to collect required execution information from context. In call sequence, it occurs after Kernel execution function call.

> Special note: Under current execution logic, even if Kernel execution function returns error, this function will still be called. Therefore, function implementation needs extra care for Kernel output processing, must strictly match Kernel execution function. Under current call timing not modified and unable to know execution result, developers need to carefully implement TracePrinter function to ensure no runtime errors occur when Kernel execution function exits abnormally, even though this is hard to implement. In the future, this function's call timing will likely be modified, or execution result information will be passed when calling.

Based on current call timing, we only suggest two parts of testing for TracePrinter:
- Test case 1: Test return information after Kernel execution function executes normally
```c++
  registry.FindKernelFuncs("LaunchKernelWithFlag")->run_func(context.WithFlag())
  auto ret = registry.FindKernelFuncs("LaunchKernelWithFlag")->trace_printer(context.WithFlag());
  EXPECT_FALSE(ret.empty());
```
> Note: This test is insufficient, assuming that when handling execution function errors, no modifications are made to output, which is usually not true.

- Test case 2: Simulate and test return information when Kernel execution function is abnormal (TBD)
```
Construct ->run_func() failure scenario, then call ->trace_printer method
```
