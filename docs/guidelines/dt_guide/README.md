# GE DT用例开发总纲

本文针对UT和ST两类测试，结合DT测试中常见的问题，确定用例规范、基本要求。本文默认读者已经有了基本的UT/ST开发经验，可以实现UT/ST用例，因此关于用例的基础写法不会做详细介绍。

> 本文仅指导如何写UT/ST，请使用`ge-dt-runner`skill编译运行，或参考docs/build.md全量执行用例

## 详细指导

| 文档 | 内容 | 适用场景 |
|------|------|---------|
| [测试框架指南](测试框架指南.md) | 测试基类、Graph DSL、faker、stub、checker | 构造输入、打桩、校验输出时查阅 |
| [UT用例开发指导](UT用例开发指导.md) | UT设计checklist、校验方法、图类UT、rt2 kernel UT | 编写UT用例时查阅 |
| [ST用例开发指导](ST用例开发指导.md) | ST注释要求、场景checklist、图编译/执行校验、DUMP阶段列表 | 编写ST用例时查阅 |

## 基础知识

### 何时新增DT测试用例

一般来说，在两种情况下新增DT测试用例：

1. 新需求开发时，基于模块设计，设计并新增UT/ST用例

2. 修复问题时，包含自验证发现的问题、上库时流水线发现的问题、上库后被提单的问题等等。理论上只要发现了一个bug，并且你在修复它，就需要新增DT测试用例，根据被测的行为，你可以选择新增UT用例或ST用例，或者两者都各增加几个。建议的问题修复流程为：

   ```mermaid
   flowchart LR
   	发现bug-->分析跟因-->通过UT/ST复现问题-->修复代码-->确认问题用例被修复-->上库
   ```

### DT测试工程

GE仓UT/ST代码位于tests目录下，tests目录下的子目录作用：

```yaml
tests
 - acl_ut: ACL接口的UT用例目录
 - autofuse: 自动融合相关的UT/ST用例目录
 - benchmark: 性能用例的目录
 - bin: 测试可执行产物及kernel二进制目录
 - cmake: 测试工程的公共cmake脚本
 - depends: 底层依赖的桩代码和测试辅助工具，各子目录说明如下：
   - slog: 日志接口桩（slog、dlog、ascendalog）
   - runtime: 昇腾运行时桩（rtMalloc、rtMemcpy、rtLaunch、stream/event等）
   - hccl: 集合通信桩（HCCL AllReduce等）
   - ascendcl: AscendCL API桩（acl_rt、设备管理、内存等）
   - op_stub: 算子实现桩，提供fake算子实现供测试图执行器调度
   - profiler: Profiling/Dump桩
   - platform: 平台/芯片信息查询桩
   - error_manager: 错误管理桩
   - graph_tuner: 图调优桩
   - aoe: AOE（Ascend Optimization Engine）桩
   - mmpa/mmpa2: 平台无关的内存/进程抽象API桩
   - llm_datadist: LLM数据分发测试辅助桩
   - helper_runtime: 辅助运行时桩（tsd_client、grpc_server等）
   - aihacb_autofusion: 自动融合桩
   - python: Python绑定的LLM封装模块
   - checker: 仅头文件的测试校验工具（mem_trace_checker、shape_checker等）
   - conf: 测试配置数据（如error_code.json）
   - symbol: 仅头文件的符号形状推断测试辅助工具
 - dflow: dflow模块的UT/ST用例目录（含flow_graph、llm_datadist、pydflow、runner、udf等子模块）
 - docs: 测试相关文档目录
 - engines: 各执行引擎的UT/ST用例目录（含cpueng、dvppeng、ffts_engine、hccl_engine、nn_engine、rts_engine、te_fusion）
 - framework: 框架目录，包含UT/ST的公用测试代码，例如faker、stub、easy_graph等
 - ge: GE核心模块的UT/ST用例目录（含ut、st子目录）
 - graph_metadef: 图元数据定义模块的UT/benchmark用例目录
 - parser: 模型解析模块的UT/ST用例目录
 - python_tests: Python接口测试目录
 - test_c: C接口测试目录
```

不论是UT还是ST，我们均使用googletest测试框架，并提供了AddressSanitizer来做内存类检查、gcov来做覆盖率统计。对于新增代码，UT覆盖率要求超过90%，ST覆盖率要求超过80%。覆盖率统计的具体操作请参考docs/build.md，基本命令为：
```bash
# 带 -c 参数运行测试，自动生成覆盖率统计文件到 cov/ 目录
bash tests/run_test.sh -c [其他参数]
```
前置条件：需安装 `lcov`（`sudo apt-get install lcov`）和 `pip3 install coverage`，且编译和运行环境的 gcc/gcov 版本需一致。

## 新用例文件加入到构建系统

GE测试工程使用标准CMake构建，没有自定义的注册宏。新增用例文件后，需修改对应目录的CMakeLists.txt。
**将源文件加入构建变量**
```cmake
# 方式一：添加到显式列出的源文件列表中
set(MY_TEST_FILES
    "graph/optimize/foo_unittest.cc"
)

# 方式二：已有 file(GLOB_RECURSE ...) 时，新文件会被自动收录
```

## 用例规范

### 规范1：UT用例文件名为"所测文件名_unittest.cc"

例如有一个类为`Foo`，其位于`foo.cc`中，那么对应的UT用例文件名为: `foo_unittest.cc`。

如果使用googletest的测试套+测试用例的方式，那么测试套的类名为`FooUT`，代码说明：

```c++
// foo_unittest.cc

#include <gtest/gtest.h>
class FooUT : public testing::Test {};  // 测试套定义，类名为 FooUT

TEST_F(FooUT, case_name) {  // 测试用例定义
  // ...
}
```

使用规范化的命名，可以让后续开发者查阅用例时更容易找到对应文件，所以务必遵守。

### 规范2：ST用例文件名为"所测特性_systemtest.cc"

例如零拷贝的ST用例可以被命名为：`zero_copy_systemtest.cc`，`zero_copy_system_test.cc`也是被接受的。ST用例的测试粒度为**特性**，不要为了覆盖率而用覆盖率来测试某个**模块**，例如`tensor_systemtest.cc`。

### 规范3：UT用例名

UT用例名使用如下模板：`被测接口_预期行为_场景`，被下划线分割的每一部分，均使用大驼峰命名。例如对`FrameSelector`的测试中，希望测试接口`SelectMainRoot`，当前图为Main图的子图时，预期行为是，调用此接口后，创建的Node仍然位于Main图上，用例名为：`SelectMainRoot_CreateOnMainRoot_CurrentFrameIsMainSubgraphs`。

有时仅期望接口成功，那么用例名也可以简写为`被测接口_场景Ok`，例如测试Tensor的默认构造可以成功构建出Tensor，用例名可以为`DefaultConstructor_ConstructOk`。

有时一个模块分为很多子功能，此时用例名的第一个字段也可以用子功能的名字：`子功能_预期行为_场景`。

### 规范4：一个用例只校验一个场景

不论是UT还是ST用例，一个用例仅应该构造一个场景，并将其校验完整。如果有其他场景，需要新写一个用例测试。这样做的缺点是可能存在部分的代码冗余和牺牲一些运行效率，换来的优点是后期易于维护。

举例说明，如下用例是一个正面例子，该用例通过构造函数构造了Tensor，并完整校验了默认值是否符合预期：
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

可以接受的例子：虽然在一个用例中校验了多个子场景，但还好都是校验空指针场景；
```c++
TEST_F(ModelOutpusUT, UpdateOutputShape_Failed_WhenNullptr) {
  StorageShape shape = {{8, 3, 224, 224}, {8, 1, 224, 224, 16}};
  kernel::BuildTensorAttr attr = {kOnHost, ge::DT_FLOAT16, {ge::FORMAT_NCHW,  ge::FORMAT_NC1HWC0, {}}};
  auto tensor = TensorFaker().Shape({}).Format(ge::FORMAT_ND).DataType(ge::DT_FLOAT).Build();

  // 首参数为空
  auto context = KernelRunContextFaker().Inputs({nullptr, tensor.GetTensor(), &attr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);

  // 第二个参数为空
  context = KernelRunContextFaker().Inputs({&shape, nullptr, &attr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);

  // 第三个参数为空
  context = KernelRunContextFaker().Inputs({&shape, tensor.GetTensor(), nullptr}).Build();
  ASSERT_NE(kernel::UpdateOutputShape(context.GetContext()), ge::GRAPH_SUCCESS);
}
```

如下例子不可接受：
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
上述例子中，测试了SinkBin可以成功、测试了多次Sink同一个bin会做cache，测试了empty key的场景，这导致此测试用例的测试点很分散、测试意图不清晰、用例名也无法取得简单，导致了后续的维护困难。

### 规范5：不允许将private define为public

不论是UT还是ST，均是基于公开接口测试，即使是UT，也是基于模块的公开接口做测试。private函数、成员都是模块的内部实现，不应该成为测试或校验点的范围。这是为了用例的稳定性考虑，因为我们认为，一个模块的行为是相对稳定的，但是模块的内部实现是可以随意变化的。

### 规范6：用例中不允许打开Debug级别日志或Dump图开关

Debug级别日志、Dump图开关一般来说是用于开发者调试的工具，打开Debug级别日志或Dump图开关会极大地影响代码的执行速度，在一个用例中打开了上述开关，影响到此用例及其后继执行的所有用例，进而导致整个UT/ST工程执行速度很慢。因此请不要打开此开关，如果为了调试方便，那么仅在调试时打开，上库时需要将此开关关闭

例外：

* 针对Debug日志、Dump图测试时，可以打开开关，但是需要在用例结束时关闭

### 规范7：用例不可以有副作用 

一个用例的执行不应该对其他用例产生影响，也不应该对自己产生影响。例如：

* 如果用例中产生或修改了磁盘文件，那么在TearDown中，需要将对应文件删除或恢复
* 如果用例中新增或修改了环境变量，那么在TearDown中，需要将其恢复
* 如果用例中改变了单例或全局变量的状态，那么在TearDown中，需要恢复

### 规范8：不可以依赖外部的初始化动作

用例的初始化/去初始化操作应该位于本用例的setup/teardown函数中进行，一个用例不应该依赖于外部的初始化。例如部分用例需要在执行该用例前，将某些so拷贝到指定路径才可以正确执行。如果一个用例依赖于某些so在特定路径下，该用例应该在自己的setup函数中完成so的拷贝、在teardown中完成so的删除。
