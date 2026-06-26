# GE DT Test Case Development Guide

This document establishes test case standards and basic requirements for UT and ST testing, addressing common issues encountered in DT testing. This document assumes readers have basic UT/ST development experience and can implement UT/ST test cases, so basic test case writing will not be covered in detail.

> This document only guides how to write UT/ST. Use `ge-dt-runner` skill to compile and run, or refer to docs/build.md for full test execution.

## Detailed Guides

| Document | Content | Applicable Scenarios |
|------|------|---------|
| [Test Framework Guide](./test_framework_guide.md) | Test base classes, Graph DSL, faker, stub, checker | Reference when constructing inputs, stubbing, and validating outputs |
| [UT Case Development Guide](./UT_testcase_dev_guide.md) | UT design checklist, validation methods, graph UT, rt2 kernel UT | Reference when writing UT cases |
| [ST Case Development Guide](./ST_testcase_dev_guide.md) | ST annotation requirements, scenario checklist, graph compilation/execution validation, DUMP phase list | Reference when writing ST cases |

## Fundamentals

### When to Add DT Test Cases

Generally, DT test cases are added in two scenarios:

1. During new requirement development, design and add UT/ST cases based on module design

2. When fixing issues, including self-verified issues, issues found by pipelines during submission, and issues reported after submission. Theoretically, whenever a bug is discovered and you're fixing it, DT test cases should be added. Based on the tested behavior, you can choose to add UT cases or ST cases, or both. The recommended bug fix workflow is:

   ```mermaid
   flowchart LR
   	Discover bug-->Analyze root cause-->Reproduce issue via UT/ST-->Fix code-->Confirm issue case is fixed-->Submit
   ```

### DT Test Engineering

GE repository UT/ST code is located in the tests directory. The subdirectories under tests serve the following purposes:

```yaml
tests
 - acl_ut: UT case directory for ACL interfaces
 - autofuse: UT/ST case directory for automatic fusion
 - benchmark: Performance case directory
 - bin: Test executable artifacts and kernel binary directory
 - cmake: Common cmake scripts for test engineering
 - depends: Stub code for underlying dependencies and test helpers. Subdirectories are described below:
   - slog: Log interface stubs (slog, dlog, ascendalog)
   - runtime: Ascend runtime stubs (rtMalloc, rtMemcpy, rtLaunch, stream/event, etc.)
   - hccl: Collective communication stubs (HCCL AllReduce, etc.)
   - ascendcl: AscendCL API stubs (acl_rt, device management, memory, etc.)
   - op_stub: Operator implementation stubs, providing fake operator implementations for test graph executor scheduling
   - profiler: Profiling/Dump stubs
   - platform: Platform/chip information query stubs
   - error_manager: Error management stubs
   - graph_tuner: Graph tuning stubs
   - aoe: AOE (Ascend Optimization Engine) stubs
   - mmpa/mmpa2: Platform-independent memory/process abstraction API stubs
   - llm_datadist: LLM data distribution test helper stubs
   - helper_runtime: Helper runtime stubs (tsd_client, grpc_server, etc.)
   - aihacb_autofusion: Automatic fusion stubs
   - python: Python binding LLM wrapper module
   - checker: Header-only test validation tools (mem_trace_checker, shape_checker, etc.)
   - conf: Test configuration data (e.g., error_code.json)
   - symbol: Header-only symbolic shape inference test helper tools
 - dflow: UT/ST case directory for dflow module (including flow_graph, llm_datadist, pydflow, runner, udf submodules)
 - docs: Test-related documentation directory
 - engines: UT/ST case directories for various execution engines (including cpueng, dvppeng, ffts_engine, hccl_engine, nn_engine, rts_engine, te_fusion)
 - framework: Framework directory, containing common test code for UT/ST, such as faker, stub, easy_graph, etc.
 - ge: UT/ST case directory for GE core module (including ut, st subdirectories)
 - graph_metadef: UT/benchmark case directory for graph metadata definition module
 - parser: UT/ST case directory for model parsing module
 - python_tests: Python interface test directory
 - test_c: C interface test directory
```

Both UT and ST use the googletest testing framework and provide AddressSanitizer for memory checking and gcov for coverage statistics. For new code, UT coverage should exceed 90%, and ST coverage should exceed 80%. For specific coverage statistics operations, refer to docs/build.md. The basic command is:
```bash
# Run tests with -c parameter, automatically generate coverage statistics files to cov/ directory
bash tests/run_test.sh -c [other parameters]
```
Prerequisite: Need to install `lcov` (`sudo apt-get install lcov`) and `pip3 install coverage`, and gcc/gcov versions must be consistent in build and runtime environments.

## Adding New Test Case Files to Build System

GE test engineering uses standard CMake build system without custom registration macros. After adding new case files, modify the CMakeLists.txt in the corresponding directory.
**Add source files to build variables**
```cmake
# Method 1: Add to explicitly listed source file list
set(MY_TEST_FILES
    "graph/optimize/foo_unittest.cc"
)

# Method 2: When file(GLOB_RECURSE ...) exists, new files will be automatically included
```

## Test Case Standards

### Standard 1: UT Case File Name Should Be "Tested_File_Name_unittest.cc"

For example, if there's a class `Foo` located in `foo.cc`, the corresponding UT case file name should be: `foo_unittest.cc`.

If using googletest's test suite + test case approach, the test suite class name should be `FooUT`. Code example:

```c++
// foo_unittest.cc

#include <gtest/gtest.h>
class FooUT : public testing::Test {};  // Test suite definition, class name is FooUT

TEST_F(FooUT, case_name) {  // Test case definition
  // ...
}
```

Using standardized naming makes it easier for subsequent developers to find corresponding files when reviewing cases, so please comply strictly.

### Standard 2: ST Case File Name Should Be "Tested_Feature_systemtest.cc"

For example, zero-copy ST cases can be named: `zero_copy_systemtest.cc`, `zero_copy_system_test.cc` is also acceptable. ST case granularity is **feature**, do not use coverage to test a **module** for coverage purposes, such as `tensor_systemtest.cc`.

### Standard 3: UT Case Naming

UT case names use the following template: `Tested_Interface_Expected_Behavior_Scenario`, where each underscore-separated part uses PascalCase. For example, when testing `FrameSelector`, you want to test the `SelectMainRoot` interface, when the current graph is a subgraph of Main graph, the expected behavior is that after calling this interface, the created Node is still on the Main graph, the case name would be: `SelectMainRoot_CreateOnMainRoot_CurrentFrameIsMainSubgraphs`.

Sometimes you only expect the interface to succeed, then the case name can be simplified to `Tested_Interface_ScenarioOk`, for example, testing Tensor default constructor can successfully construct a Tensor, the case name can be `DefaultConstructor_ConstructOk`.

Sometimes a module has many sub-functions, then the first field of the case name can also use the sub-function name: `Sub_Function_Expected_Behavior_Scenario`.

### Standard 4: One Test Case Should Only Validate One Scenario

Whether UT or ST cases, one case should only construct one scenario and validate it completely. If there are other scenarios, a new case should be written. The downside of this approach is some code redundancy and sacrifice of runtime efficiency, but the advantage is easier maintenance later.

For example, the following case is a good example. This case constructs a Tensor through the constructor and completely validates whether the default values meet expectations:
```c++
TEST_F(TensorUT, ConstructOk_V2) {
  TensorV2 tensor{{{8, 3, 224, 224}, {16, 3, 224, 224}},       // shape
                  {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},  // format
                  kOnDeviceHbm,                                // placement
                   ge::DT_FLOAT16,                              // dt
                  nullptr};
  const TensorV2 &t2 = tensor;

  EXPECT_EQ(t2.GetOriginShape(), Shape({8, 3, 224, 224}));
  EXPECT_EQ(t2.GetStorageShape(), Shape({16, 3, 224, 224}));

  EXPECT_EQ(t2.GetOriginFormat(), ge::FORMAT_ND);
  EXPECT_EQ(t2.GetStorageFormat(), ge::FORMAT_FRACTAL_NZ);
  StorageFormat storage_format{ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}};
  EXPECT_EQ(t2.GetFormat(), storage_format);
  EXPECT_EQ(t2.GetExpandDimsType(), ExpandDimsType{});

  EXPECT_EQ(t2.GetPlacement(), kOnDeviceHbm);
  EXPECT_EQ(t2.GetDataType(), ge::DT_FLOAT16);
  EXPECT_EQ(t2.GetAddr(), nullptr);
  EXPECT_EQ(t2.GetData<int64_t>(), nullptr);
}
```

Acceptable example: Although multiple sub-scenarios are validated in one case, they're all validating null pointer scenarios;
```c++
TEST_F(ModelOutpisUT, UpdateOutputShape_Failed_WhenNullptr) {
  StorageShape shape = {{8, 3, 224, 224}, {8, 1, 224, 224, 16}};
  kernel::BuildTensorAttr attr = {kOnHost, ge::DT_FLOAT16, {ge::FORMAT_NCHW,  ge::FORMAT_NC1HWC0, {}}};
  auto tensor = TensorFaker().Shape({}).Format(ge::FORMAT_ND).DataType(ge::DT_FLOAT).Build();

  // First parameter is null
  auto context = KernelRunContextFaker().Inputs({nullptr, tensor.GetTensor(), &attr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);

  // Second parameter is null
  context = KernelRunContextFaker().Inputs({&shape, nullptr, &attr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);

  // Third parameter is null
  context = KernelRunContextFaker().Inputs({&shape, tensor.GetTensor(), nullptr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);
}
```

The following example is not acceptable:
```c++
TEST_F(SinkNodeBinTest, test_sink_node_bin_with_handle_success) {
  kernel::BinData bin_data;
  const char *bin_key1 = "key1";
  const char *bin_key2 = "key2";
  const char *empty_key = "";

  struct FakeRuntime : RuntimeStubImpl {
    rtError_t rtRegisterAllKernel(const rtDevBinary_t *bin, void **handle) {
      static size_t registed_num = 0x10;
      *handle = (void *)(registed_num++);
      return RT_ERROR_NONE;
    }
  };
  GertRuntimeStub runtime(std::unique_ptr<RuntimeStubImpl>(new FakeRuntime()));

  auto run_context = BuildKernelRunContext(2, 1);
  run_context.value_holder[0].Set((void *)&bin_data, nullptr);
  run_context.value_holder[1].Set((void *)bin_key1, nullptr);
  ASSERT_EQ(kernel::SinkNodeBinWithHandle(run_context.GetContext<KernelContext>()), ge::GRAPH_SUCCESS);
  ASSERT_EQ(run_context.value_holder[2].GetValue<uint64_t>(), 0x10);

  run_context = BuildKernelRunContext(2, 1);
  run_context.value_holder[0].Set((void *)&bin_data, nullptr);
  run_context.value_holder[1].Set((void *)bin_key2, nullptr);
  ASSERT_EQ(kernel::SinkNodeBinWithHandle(run_context.GetContext<KernelContext>()), ge::GRAPH_SUCCESS);
  ASSERT_EQ(run_context.value_holder[2].GetValue<uint64_t>(), 0x10 + 1);

  run_context = BuildKernelRunContext(2, 1);
  run_context.value_holder[0].Set((void *)&bin_data, nullptr);
  run_context.value_holder[1].Set((void *)bin_key1, nullptr);
  ASSERT_EQ(kernel::SinkNodeBinWithHandle(run_context.GetContext<KernelContext>()), ge::GRAPH_SUCCESS);
  ASSERT_EQ(run_context.value_holder[2].GetValue<uint64_t>(), 0x10); // cached

  run_context = BuildKernelRunContext(2, 1);
  run_context.value_holder[0].Set((void *)&bin_data, nullptr);
  run_context.value_holder[1].Set((void *)empty_key, nullptr);
  ASSERT_EQ(kernel::SinkNodeBinWithHandle(run_context.GetContext<KernelContext>()), ge::GRAPH_SUCCESS);
  ASSERT_EQ(run_context.value_holder[2].GetValue<uint64_t>(), 0x10 + 2);

  run_context = BuildKernelRunContext(2, 1);
  run_context.value_holder[0].Set((void *)&bin_data, nullptr);
  run_context.value_holder[1].Set((void *)empty_key, nullptr);
  ASSERT_EQ(kernel::SinkNodeBinWithHandle(run_context.GetContext<KernelContext>()), ge::GRAPH_SUCCESS);
  ASSERT_EQ(run_context.value_holder[2].GetValue<uint64_t>(), 0x10 + 3);
}
```
In the above example, it tests that SinkBin can succeed, tests that sinking the same bin multiple times does caching, and tests empty key scenarios. This causes the test case's test points to be scattered, unclear test intent, and difficult to name simply, leading to maintenance difficulties later.

### Standard 5: Do Not Define Private Functions as Public

Whether UT or ST, testing should be based on public interfaces. Even for UT, testing should be based on the module's public interfaces. Private functions and members are internal implementations of the module and should not be within the scope of testing or validation points. This is for case stability, as we believe a module's behavior is relatively stable, but the module's internal implementation can change arbitrarily.

### Standard 6: Do Not Enable Debug Level Logging or Graph Dump Switches in Test Cases

Debug level logging and graph dump switches are generally tools for developer debugging. Enabling debug level logging or graph dump switches greatly affects code execution speed. Enabling these switches in a case affects this case and all subsequently executed cases, thereby causing the entire UT/ST project to run slowly. Therefore, do not enable these switches. If for debugging convenience, only enable during debugging and disable when submitting.

Exceptions:

* When testing debug logging and graph dumps, switches can be enabled, but they need to be disabled when the case ends

### Standard 7: Test Cases Must Not Have Side Effects

A test case's execution should not affect other cases, nor should it affect itself. For example:

* If the case creates or modifies disk files, the corresponding files should be deleted or restored in TearDown
* If the case adds or modifies environment variables, they should be restored in TearDown
* If the case changes singleton or global variable states, they should be restored in TearDown

### Standard 8: Do Not Rely on External Initialization Actions

Test case initialization/deinitialization operations should be in the case's own setup/teardown functions. A case should not depend on external initialization. For example, some cases require certain .so files to be copied to a specific path before the case can execute correctly. If a case depends on certain .so files being in a specific path, the case should complete the .so copy in its own setup function and complete the .so deletion in teardown.
