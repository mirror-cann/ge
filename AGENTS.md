# AGENTS.md

本文件为 agent 在此代码仓库中工作时提供指导。

## 项目概述

GE (Graph Engine) 是华为 CANN (Compute Architecture for Neural Networks) 的图引擎 - 面向昇腾 AI 处理器的高性能图编译器和执行器。GE 提供图优化、多流并行执行、内存复用优化和模型下沉等能力。

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

### DT 测试开发

**使用技能**: `ge-dt-developer`
**适用场景**: 编写、新增 UT/ST 测试用例。

### GE 单元测试 / 系统测试

**使用技能**: `ge-dt-runner`
**适用场景**: 编译和运行 GE 项目的单元测试(UT)和系统测试(ST)。

### 清理构建产物
**非必要，不清理。尽量使用增量编译，能省90%编译时间**
```bash
rm -rf build_ut/ build_st/ output/ build/ build_out/ cov/ build_cmake_gcov/
```

## 架构文档加载

**重要**： 在探索仓库/项目、回答问题、修改代码、输出设计文档、需求Spec或做代码检视时，根据下表加载对应文档。每个文档只需加载一次，匹配触发词、涉及目录或场景中任意一条即加载。同时，修改了代码后，要更新`docs/architecture/features/`和`docs/architecture/modules`下的对应文档。

| 文档 | 触发词 | 涉及目录 |
|------|----------------|----------|
| [`architecture.md`](docs/architecture/architecture.md) | 架构总览、整体设计、GE介绍、系统架构、编译优化、插件扩展 | 首次了解项目时 |
| [`ascend-ir.md`](docs/architecture/modules/graph_metadef/ascend-ir.md) | AscendIR、图结构、算子注册、Anchor、DAG | `base/`、`inc/`（图结构相关） |
| [`compiler.md`](docs/architecture/modules/compiler/compiler.md) | 编译器、优化pass、融合、引擎分区、算子编译 | `compiler/`（非 memory/split/stream 子目录） |
| [`runtime.md`](docs/architecture/modules/runtime/runtime.md) | 动态运行执行器、模型加载、模型执行、Hybrid、v2架构 | `runtime/`（整体架构层面） |
| [`datadump.md`](docs/architecture/features/datadump.md) | dump、溢出、落盘、datadump、异常dump | `common/dump/`、`runtime/*/dump/` |
| [`external_weight.md`](docs/architecture/features/external_weight.md) | 外置权重、external weight、FileConstant、权重分离、权重落盘 | 仅使用触发词 |
| [`constant_folding.md`](docs/architecture/features/constant_folding.md) | 常量折叠、constant folding、常量折叠优化、常量表达式求值 | `*constant_folding*` |
| [`dynamic_gear.md`](docs/architecture/features/dynamic_gear.md) | 动态分档、dynamic gear、档位、动态batch、动态分辨率、dynamic_dims | `compiler/graph/preprocess/`、`compiler/graph/passes/multi_batch/`、`runtime/v1/executor/` |
| [`memory_conflict.md`](docs/architecture/features/memory_conflict.md) | 内存冲突、内存排布冲突、读写冲突、Inplace冲突、子图地址隔离 | `compiler/graph/passes/memory_conflict/`、`mem_rw_conflict_optimize.cc`、`compiler/graph/optimize/mem_layout_conflict_optimize/`、`mem_inplace.cc` |
| [`model_cache.md`](docs/architecture/features/model_cache.md) | 模型缓存、model cache、编译缓存、OM缓存、JIT缓存 | `*model_cache*` |
| [`profiling.md`](docs/architecture/features/profiling.md) | profiling、性能分析、性能采集、msprof、性能调优 | `*profiling*` |
| [`so_in_om.md`](docs/architecture/features/so_in_om.md) | SO in OM、算子打包、so打包、自包含模型、算子依赖 | `*op_so_store*` |
| [`tensormove_delete.md`](docs/architecture/features/tensormove_delete.md) | TensorMove消除、TensorMove删除、冗余拷贝消除 | `*tensor_move_delete*` |
| [`variable_manager.md`](docs/architecture/features/variable_manager.md) | 变量管理、Variable、变量内存、VarRef、变量生命周期、常量、FileConstant、外置权重 | `*var_manager*`、`*variable_optimize*` |
| [`zero_copy.md`](docs/architecture/features/zero_copy.md) | 零拷贝、zero copy、模型输入/输出、用户输入/输出、用户内存/地址 | `*zero_copy*` |
| [`concat_no_task.md`](docs/architecture/features/concat_no_task.md) | Concat No Task、concat优化、连续内存拼接、虚拟算子、不生成Task | `*concat_notask*` |

| 关键特性设计原则和软件约束 | 触发词 | 涉及目录 |
|------|----------------|----------|
| [`memory-constraints.md`](docs/architecture/constraints/memory-constraints.md) | 显存、内存复用、block_mem、allocator、零拷贝、连续内存、内存排布冲突、内存释放 | `compiler/graph/build/memory/`、`compiler/graph/optimize/mem_layout_conflict_optimize/` |
| [`rt2_runtime.md`](docs/architecture/constraints/rt2_runtime.md) | RT2、动态shape、rt2 executor、hybrid执行 | `runtime/v2/` |
| [`known_shape_runtime.md`](docs/architecture/constraints/known_shape_runtime.md) | 静态shape、known shape、davinci model、sink模式、地址刷新 | `runtime/v1/` |
| [`graph_split.md`](docs/architecture/constraints/graph_split.md) | 图拆分、切图、cluster、动态图拆分、执行器选择 | `compiler/graph/split/` |
| [`stream_allocator.md`](docs/architecture/constraints/stream_allocator.md) | 流分配、stream、多流、流复用、event同步、流激活 | `compiler/graph/build/stream/` |
| [`graph_metadef.md`](docs/architecture/constraints/graph_metadef.md) | 图基础结构 | `graph_metadef/` |


## 开发规范
**gitcode pr/issue 操作**
@.claude/skills/default-skills/SKILL.md

### 代码检视检查项（Code Review Checklist）

> **触发词**：代码检视、代码审查、code review、review 代码、检视代码、PR 审查、CR、代码走查、检视意见、review comment

进行代码检视时，**必须**逐项检查以下内容，并输出检查结果：

- [ ] **编码红线**：读取 [编码红线.md](docs/guidelines/编码红线.md)，逐条核对修改是否违反红线规则
- [ ] **关键特性设计原则和软件约束**：根据修改涉及的目录，加载上表"关键特性设计原则和软件约束"中对应的架构文档，确认修改不违反已有设计约束（如内存模型、流分配规则、图拆分策略等），也不要一次性加载全部文档，避免占用过多context，而是匹配触发词、涉及目录或场景中任意一条即加载。。
- [ ] **跨特性交叉影响（cross-feature-check）**：检查本次修改是否影响其他特性的行为。**必须**先读取 [cross_feature_check.md](docs/guidelines/cross_feature_check.md)，按其指引逐场景分析。具体步骤：
  1. 识别本次修改涉及的所有模块/目录
  2. 在"关键特性设计原则和软件约束"文档表中查找与这些模块相关的**其他特性文档**
  3. 按 cross_feature_check.md 中的场景表逐项评估适用性和影响
  4. 输出评估结论，如有风险需明确标注

**示例输出格式**：
```
### 代码检视检查结果
- [x] 编码红线：已检查，无违反
- [x] 关键特性设计原则：已加载 memory-constraints.md，修改符合内存复用约束
- [x] 跨特性交叉影响：已按 cross_feature_check.md 逐场景分析，
      修改涉及 compiler/graph/build/memory/，已检查 stream_allocator.md，
      确认本次内存分配改动不影响流分配逻辑
```

### 设计文档检查项（Design Document Checklist）

> **触发词**：设计文档、设计spec、spec输出、design document、设计方案输出、brainstorming 输出文档、写入设计文档、写spec、写设计、保存spec、save spec、save design、写入 docs/superpowers/specs、设计方案、架构设计、技术方案

任何输出设计文档/spec的场景（包括但不限于 superpowers brainstorming skill、用户直接要求写设计文档、输出设计方案），**必须**先读取模板文件 [docs/guidelines/design_document_template.md]，然后按照模板格式输出。模板中的每个章节都必须覆盖。即使 superpowers skill 有自己的格式要求，也要以本模板为准。

同时，**必须**逐项检查以下内容：

- [ ] **跨特性交叉影响（cross-feature-check）**：设计方案涉及的所有模块/目录，**必须**先读取 [cross_feature_check.md](docs/guidelines/cross_feature_check.md)，按其指引逐场景分析。按 cross_feature_check.md 中的场景表逐项评估是否存在遗漏特性/场景，并在设计文档中明确说明
- [ ] **关键特性设计原则和软件约束**：根据设计方案涉及的目录，加载上表"关键特性设计原则和软件约束"中对应的架构文档，确保设计方案与已有约束一致。如需突破已有约束，必须明确说明原因和影响范围

**示例输出格式**：
```
### 设计文档检查结果
- [x] 跨特性交叉影响：已按 cross_feature_check.md 逐场景分析，
      方案涉及 runtime/v2/ 和 compiler/graph/build/memory/，
      已检查 rt2_runtime.md 和 memory-constraints.md，方案与现有内存模型兼容，
      不影响 RT2 动态 shape 执行流程
- [x] 关键特性设计原则：已加载 rt2_runtime.md，方案遵循 hybrid 执行约束，
      无突破已有设计原则
```

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

## 编码前先思考

**不要想当然。不要掩饰困惑。把权衡摆在明面上。**

实施前：
- 明确陈述你的假设。如果不确定，就问。
- 如果存在多种理解，全部列出来——不要默默选一个。
- 如果有更简单的方案，说出来。必要时提出反对意见。
- 如果有不清楚的地方，停下来。指出困惑之处。提问。

## 简洁优先

**用最少的代码解决问题。不做任何推测性设计。**

- 不实现超出需求的功能。
- 不为一次性代码做抽象。
- 不添加未经要求的"灵活性"或"可配置性"。
- 不为不可能发生的场景做错误处理。
- 如果你写了200行而50行就够了，重写。

问问自己："高级工程师会认为这太复杂了吗？" 如果是，就简化。

## 精准修改

**只改必须改的。只清理自己弄乱的。**

编辑现有代码时：
- 不要"改进"相邻的代码、注释或格式。
- 不要重构没有问题的东西。
- 匹配既有风格，即使你会用不同的写法。
- 如果你注意到无关的死代码，提出来——不要删除它。

当你的修改产生了孤立代码时：
- 删除因你的修改而变得未使用的导入/变量/函数。
- 不要删除之前就存在的死代码，除非被要求。

检验标准：每一处改动都应该能追溯到用户的请求。

## 目标驱动执行

**定义成功标准。循环验证直到达标。**

将任务转化为可验证的目标：
- "添加校验" → "为无效输入编写测试，然后让它们通过"
- "修复bug" → "编写能复现该bug的测试，然后让它通过"
- "重构X" → "确保重构前后测试都通过"

对于多步骤任务，简要说明计划：
```
1. [步骤] → 验证: [检查项]
2. [步骤] → 验证: [检查项]
3. [步骤] → 验证: [检查项]
```

明确的成功标准让你能够独立循环迭代。模糊的标准（"让它能用"）则需要不断沟通确认。
