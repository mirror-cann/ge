# AGENTS.md

本文件为 agent 在此代码仓库中工作时提供指导。

## 项目概述

GE (Graph Engine) 是华为 CANN (Compute Architecture for Neural Networks) 的图引擎 - 面向昇腾 AI 处理器的高性能图编译器和执行器。GE 提供图优化、多流并行执行、内存复用优化和模型下沉等能力。

## 构建命令
### 基础构建
```bash
# 构建所有组件（ge_compiler、ge_executor、dflow）
bash build.sh

# 构建特定组件
bash build.sh --ge_compiler    # 构建编译器包
bash build.sh --ge_executor    # 构建执行器包
bash build.sh --dflow          # 构建 dflow 包
```

### 构建选项
```bash
bash build.sh -j16                  # 16个并行线程（默认：8）
bash build.sh --build_type=Debug    # Debug 构建（默认：Release）
bash build.sh --verbose             # 详细输出
bash build.sh --asan                # 启用 AddressSanitizer
bash build.sh --cov                 # 启用代码覆盖率
bash build.sh --output_path=<PATH>  # 设置自定义输出路径
```

### 第三方依赖
```bash
bash build_third_party.sh
```

## 测试

### GE 单元测试 / 系统测试
**使用技能**: `ge-dt-runner`

**适用场景**: 编译和运行 GE 项目的单元测试(UT)和系统测试(ST)。

**指令格式**:
```
/skill ge-dt-runner <目标或参数>
```

**支持操作**:
- 编译特定测试目标
- 运行特定测试用例
- 处理测试相关依赖和环境配置

**示例**:
- "编译并运行测试" → 使用 `/skill ge-dt-runner`
- "运行某个特定用例" → 使用 `/skill ge-dt-runner <测试名称>`

### DT 测试开发经验

开发 UT/ST 用例前，务必先阅读 `docs/guidelines/dt_guide/` 下的三份指导文档。

#### 1. 在文件末尾追加代码时必须搞清 namespace 边界

C++ 测试文件通常有多层 namespace，文件末尾的 `}` 分别关闭不同的作用域。例如：

```cpp
TEST_F(FooTest, LastTest) {
  // ...
}               // <-- 关闭测试函数
}               // <-- 关闭 namespace inner
}               // <-- 关闭 namespace outer
```

新增测试必须放在**最后一个 `TEST_F` 的 `}` 之后、namespace 的 `}` 之前**。如果误插入到测试函数体内，编译会报 `local class shall not have static data member` 等错误。

**操作方法**：追加代码前，先用 Read 工具读取文件末尾 30-50 行，识别每个 `}` 的含义，确认插入点。

#### 2. 写完代码后立即编译验证

在大型 C++ 项目中，目视检查不可靠。编译器报错信息通常非常清晰，能快速定位问题。推荐流程：

1. 编辑代码
2. 立即编译（使用 `ge-dt-runner` skill）
3. 编译失败则根据报错修复，再次编译
4. 编译成功后运行测试验证逻辑正确性

不要在"看代码觉得没问题"后跳过编译。

#### 3. 先找到可复制的同类用例，再动手写

大型代码库中 80% 的测试工作来自复制已有模式。写新用例前：

1. 在 `tests/` 目录下搜索与被测代码相关的关键词（函数名、类名、特性名）
2. 优先找同目录下的 ST 文件（ST 参考 ST，UT 参考 UT）
3. 阅读已有用例的：includes、fixture 结构、构造数据的方式、校验手段
4. 在已有用例模式基础上组合新的测试场景

### 清理构建产物
```bash
rm -rf build_ut/ build_st/ output/ build/ build_out/ cov/ build_cmake_gcov/
```

### 关键目录

| 目录 | 用途 |
|------|------|
| `api/` | 公共 API 接口（ACL、ATC、session、Python 绑定） |
| `base/` | 基础组件（图结构、工具、主机 CPU 引擎） |
| `compiler/` | 图编译（分析器、引擎、图编译器、算子编译器） |
| `runtime/` | 运行时执行（C API、算子实现） |
| `dflow/` | 分布式流框架（LLM 数据分发、UDF） |
| `parser/` | 模型格式解析器（ONNX、PB、Caffe、MindSpore） |
| `graph_metadef/` | 图元数据定义和算子注册 |
| `tests/` | 综合测试套件（UT、ST、基准测试） |
| `examples/` | 使用示例和样例 |

## 架构文档加载

**重要**： 在探索仓库、回答问题、修改代码、输出设计文档、需求Spec或做代码检视时，根据下表加载对应文档。每个文档只需加载一次，匹配触发词、涉及目录或场景中任意一条即加载。

| 文档 | 触发词 | 涉及目录 |
|------|----------------|----------|
| [`architecture.md`](docs/architecture/architecture.md) | 架构总览、整体设计、GE介绍、系统架构、编译优化、插件扩展 | 首次了解项目时 |
| [`ascend-ir.md`](docs/architecture/modules/graph_metadef/ascend-ir.md) | AscendIR、图结构、算子注册、Anchor、DAG | `base/`、`inc/`（图结构相关） |
| [`compiler.md`](docs/architecture/modules/compiler/compiler.md) | 编译器、优化pass、融合、引擎分区、算子编译 | `compiler/`（非 memory/split/stream 子目录） |
| [`runtime.md`](docs/architecture/modules/runtime/runtime.md) | 动态运行执行器、模型加载、模型执行、Hybrid、v2架构 | `runtime/`（整体架构层面） |
| [`datadump.md`](docs/architecture/features/datadump.md) | dump、溢出、落盘、datadump、异常dump | `common/dump/`、`runtime/*/dump/` |
| [`external_weight.md`](docs/architecture/features/external_weight.md) | 外置权重、external weight、FileConstant、权重分离、权重落盘 | `base/common/file_constant_utils/`、`base/graph/manager/graph_external_weight_manager.cc`、`runtime/v2/engine/gelocal/file_constant_converter.cc`、`runtime/v2/kernel/ge_local_kernel/file_constant_kernel.cc` |

| 关键特性设计原则和软件约束 | 触发词 | 涉及目录 |
|------|----------------|----------|
| [`memory-constraints.md`](docs/architecture/constraints/memory-constraints.md) | 显存、内存、复用、block_mem、allocator、零拷贝、连续内存、内存排布冲突、内存释放 | `compiler/graph/build/memory/`、`compiler/graph/optimize/mem_layout_conflict_optimize/` |
| [`rt2_runtime.md`](docs/architecture/constraints/rt2_runtime.md) | RT2、动态shape、rt2 executor、hybrid执行 | `runtime/v2/` |
| [`known_shape_runtime.md`](docs/architecture/constraints/known_shape_runtime.md) | 静态shape、known shape、davinci model、sink模式、地址刷新 | `runtime/v1/` |
| [`graph_split.md`](docs/architecture/constraints/graph_split.md) | 图拆分、切图、cluster、动态图拆分、执行器选择 | `compiler/graph/split/` |
| [`stream_allocator.md`](docs/architecture/constraints/stream_allocator.md) | 流分配、stream、多流、流复用、event同步、流激活 | `compiler/graph/build/stream/` |
| [`graph_metadef.md`](docs/architecture/constraints/graph_metadef.md) | 图基础结构 | `graph_metadef/` |

## 开发规范
### gitcode pr/issue 操作
@.claude/skills/default-skills/SKILL.md

### 代码风格
- 遵循 Google 开源代码规范

## 环境要求
- GCC >= 7.3.x
- Python 3 >= 3.9.x（需额外 `pip3 install coverage`）
- CMake >= 3.16.0（推荐 3.20.0）
- 需要安装 CANN Toolkit
- 第三方库：protobuf、grpc、boost、gtest 等

## 语言
使用中文回答问题