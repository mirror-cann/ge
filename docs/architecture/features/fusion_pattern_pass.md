# 融合 Pattern Pass 机制

## 1. 这类 Pass 解决什么问题

GE 编译模型时，输入是一张计算图。图里每个节点是一个算子，节点之间的边表示 Tensor 从哪个算子流向哪个算子。

融合 Pattern Pass 的目标很直接：**在图中找到一段符合规则的小结构，然后把这段结构替换成另一段等价结构**。

例如，图里有一段 `MatMul + Add`：

```text
a ----\
       MatMul ----\
b ----/            Add ---- out
c ----------------/
```

如果目标硬件上 `GEMM` 能一次完成同样的计算，就可以把它替换为：

```text
a ----\
b ----- GEMM ---- out
c ----/
```

再比如，图里有 `Add(x, 0)`，它的输出和 `x` 等价，可以直接替换成 `x`。这类优化不会改变模型语义，但可以减少算子数量、减少中间 Tensor 读写，或把图改成后端更容易高效执行的形态。

GE 提供两种常用接口：

| 接口 | 适用场景 | 直观理解 |
|------|----------|----------|
| `PatternFusionPass` | 匹配一段子图，再替换成另一段子图 | “找到这个形状的局部图，然后整体替换” |
| `DecomposePass` | 匹配某一种单个算子，再替换成多个算子组成的子图 | “把一个复杂算子拆成几个基础算子” |

Python 和 C++ 的写法不同，但底层机制一致：都是由 GE 在编译阶段运行 pass，完成匹配、过滤、替换和重连。

开发指南：

- [Python 融合 Pass 开发指南](../../../examples/fusion_pass/python_fusion_pass_development_guide.md)
- [C++ 融合 Pass 开发指南](../../../examples/fusion_pass/cpp_fusion_pass_development_guide.md)


## 2. 一次 PatternFusionPass 如何执行

一次 `PatternFusionPass` 的执行可以拆成五步：

```text
定义 pattern -> 在目标图中匹配 -> 按条件过滤 -> 生成 replacement -> 替换并重连边
```

### 2.1 定义 pattern

`pattern` 是一张很小的模板图，用来表达“我要在真实图里找什么”。

以 `Add(x, 0)` 为例，pattern 只需要表达：

```text
外部输入 x ----\
               Add ---- pattern 输出
常量输入 0 ----/
```

这里的“外部输入”不是固定的某个真实节点，而是一个占位符。匹配时，GE 会把真实图中连到这段结构外部的 Tensor 对应到这个占位符。

### 2.2 在目标图中匹配

GE 会用 pattern 到真实图里搜索同构结构。可以把它理解为：

1. pattern 里普通算子的类型必须和真实图中的算子类型一致。
2. pattern 里的数据边必须能在真实图里找到对应连接。
3. pattern 的输入输出边界必须完整，替换后真实图仍然能连通。

匹配成功后，GE 生成一个 `MatchResult`。它记录了这次命中的真实节点、真实边，以及 pattern 中主动捕获的 Tensor。

### 2.3 用 MeetRequirements 过滤

拓扑匹配只说明“形状像”，不一定说明“可以替换”。

例如 `Add(x, Const)` 的拓扑可以匹配所有“输入加常量”的结构，但只有常量值等于 0 时，才能替换成 `x`。这个判断应放在 `MeetRequirements` 中。

`MeetRequirements` 返回：

- `true`：这次匹配满足条件，可以进入替换。
- `false`：跳过这次匹配，继续查找下一处。

### 2.4 用 Replacement 生成替换图

`Replacement` 返回另一张小图，用来替换匹配到的真实子图。

如果 pattern 是 `Add(x, 0)`，replacement 可以只返回输入 `x`：

```text
x ---- replacement 输出
```

如果 pattern 是 `MatMul + Add`，replacement 可以返回 `GEMM`：

```text
a ----\
b ----- GEMM ---- replacement 输出
c ----/
```

### 2.5 替换和重连

GE 会删除被匹配到的旧子图，插入 replacement，然后按 pattern 声明的输入输出边界，把外部消费者重新接到 replacement 上。

这里最重要的是“边界”。边界写错时，即使 pattern 能匹配，也可能无法安全替换。

## 3. Pattern 的边界规则

边界规则是写 Pattern Pass 时最容易出错的部分。可以先记住一句话：

**pattern 要完整说明这段子图从哪里接收外部输入，以及哪些输出在替换后还要交给外部使用。**

### 3.1 输入边界

凡是来自匹配子图外部的 Tensor，都要在 pattern 里用输入占位符表示。

例如要匹配 `Add(x, 0)`，`x` 来自外部，常量 `0` 在 pattern 内部创建：

```text
Data/Input ----\
                Add
Const(0) ------/
```

匹配时，`Data/Input` 可以对应真实图里的任意上游输出。

### 3.2 输出边界

凡是替换后还要被子图外部使用的 Tensor，都必须作为 pattern 的输出。

例如 pattern 是：

```text
X ---- A ---- out
```

如果真实图中只有 `A` 的输出被外部使用，那么只把 `A` 声明为 pattern 输出即可。

如果 `X` 的输出也被 pattern 外部的节点使用，就必须把 `X` 的输出也声明出来：

```text
X ---- A ---- out0
|
out1
```

否则替换后，外部节点还想使用 `X` 的输出，但 replacement 并没有为这个 Tensor 提供输出口，图会断开。GE 会尽量在匹配阶段拒绝这类不完整边界。

### 3.3 自包含约束

pattern 内部的普通节点，如果它的某个输出没有声明为 pattern 输出，那么这个输出的所有消费者都必须也在 pattern 内部。

可以按下面的问题检查：

- 这个 Tensor 替换后还会被外部节点使用吗？
- 如果会，它是否已经作为 pattern 输出声明？
- 如果不会，它的消费者是否都在 pattern 内部？

### 3.4 输入个数要精确

普通算子节点的输入个数需要和真实图一致。真实图中的某个算子有 3 个输入，pattern 里对应节点也要有 3 个输入。即使其中某些输入不关心具体来源，也要用输入占位符补齐。

### 3.5 不支持的 Pattern 内容

Pattern 匹配关注数据拓扑，不适合表达所有图结构。开发时应避免在 pattern 中使用：

| 内容 | 原因 |
|------|------|
| 控制边 | Pattern matcher 不按控制依赖做匹配 |
| 子图 | 不支持嵌套子图匹配 |
| 动态输入个数或动态输出个数的节点 | 匹配时无法确定固定的输入输出边界 |

### 3.6 多输出 Pattern

一个 pattern 可以有多个输出。多输出不是“多个 pattern”，而是“一次匹配暴露多个输出 Tensor”。

如果只是想支持多种拓扑，比如同时支持 `MatMul + Add` 和 `BatchMatMulV2 + Add`，应定义多个 pattern。

## 4. 常用扩展点

### 4.1 CaptureTensor

有时 `MeetRequirements` 或 `Replacement` 需要读取 pattern 中某个中间 Tensor 对应的真实节点，例如读取 `MatMul` 的输出描述或属性。

这时可以在定义 pattern 时捕获这个 Tensor。匹配成功后，再从 `MatchResult` 中按捕获顺序取回它。

Python `@pattern` 写法会自动捕获已访问的外部输入和 `return` 返回的 pattern 输出；如果要读取未作为输出返回的中间 Tensor，仍需要使用显式构图并调用 `Pattern.capture_tensor`。

典型用途：

- 在 `MeetRequirements` 中检查 dtype、shape、format。
- 在 `Replacement` 中读取原节点属性，写到新节点上。
- 打印命中位置，便于确认匹配结果。

参考样例：

- [C++ capture tensor 样例](../../../examples/fusion_pass/pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)
- [Python capture tensor 样例](../../../examples/fusion_pass/pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)

### 4.2 PatternMatcherConfig

默认情况下，pattern 主要匹配拓扑和算子类型。有些场景希望 matcher 更严格，例如：

- pattern 中的 Const 值必须和真实图中的 Const 值一致。
- pattern 中声明的 IR 属性和值必须和真实图一致。

这时可以启用 `PatternMatcherConfig`。

常见开关：

| 配置 | 作用 |
|------|------|
| `EnableConstValueMatch` / `enable_const_value_match` | 匹配 Const 值 |
| `EnableIrAttrMatch` / `enable_ir_attr_match` | 匹配 IR 属性和值 |

如果判断逻辑比较简单、严格且稳定，可以放进 matcher 配置；如果需要容差、dtype 归一化、多个条件组合，通常放在 `MeetRequirements` 更清晰。

参考样例：

- [C++ PatternMatcherConfig 样例](../../../examples/fusion_pass/pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md)
- [Python PatternMatcherConfig 样例](../../../examples/fusion_pass/pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)

## 5. DecomposePass 如何理解

`DecomposePass` 是一种更特殊的替换：它不需要先定义一张 pattern 图，而是直接声明“我要处理哪些算子类型”。

例如要把 `groups > 1` 的 `Conv2D` 拆成多个普通 `Conv2D`：

```text
Grouped Conv2D
      |
      v
Split(input) + Split(filter) + Conv2D * N + Concat
```

执行流程是：

```text
按 op type 找节点 -> MeetRequirements 判断这个节点是否需要拆 -> Replacement 生成替换子图
```

`DecomposePass` 适合：

- 目标是单个算子。
- 是否替换主要由这个算子的属性决定。
- replacement 会把这个算子展开成多个算子。

参考样例：

- [C++ DecomposePass 样例](../../../examples/fusion_pass/pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/cpp/README.md)
- [Python DecomposePass 样例](../../../examples/fusion_pass/pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)

## 6. Pass 执行阶段

Pass 注册时需要指定执行阶段。阶段决定 pass 能看到的图状态，也决定 replacement 是否需要自行做 shape 推导。

| 机制阶段 | Python 枚举 | C++ 枚举 | 使用建议 |
|----------|-------------|----------|----------|
| InferShape 前 | `PassStage.BEFORE_INFER_SHAPE` | `CustomPassStage::kBeforeInferShape` | 最常用。replacement 后续会进入统一 shape 推导流程 |
| InferShape 后 | `PassStage.AFTER_INFER_SHAPE` | `CustomPassStage::kAfterInferShape` | replacement 需要自行保证输出 shape 等信息正确 |
| 逻辑流分配后 | `PassStage.AFTER_ASSIGN_LOGIC_STREAM` | `CustomPassStage::kAfterAssignLogicStream` | 主要用于流相关自定义逻辑，普通融合不建议使用 |
| 内置融合后 | `PassStage.AFTER_BUILTIN_FUSION_PASS` | `CustomPassStage::kAfterBuiltinFusionPass` | 希望在 GE 内置融合完成后再处理时使用 |
| 原图优化后 | `PassStage.AFTER_ORIGIN_GRAPH_OPTIMIZE` | `CustomPassStage::kAfterOriginGraphOptimize` | 希望在原图优化结束后追加自定义处理时使用 |

初次开发建议优先选择 InferShape 前阶段。只有当你的判断必须依赖已经推导完成的 shape，或 replacement 本身明确会调用 shape 推导时，再考虑 InferShape 后阶段。

## 7. Python 与 C++ 的关系

Python 和 C++ 的 pass 都会接入 GE 的统一 pass 调度流程。

区别主要在开发体验和交付方式：

| 维度 | Python | C++ |
|------|--------|-----|
| 接入方式 | 通过 `ASCEND_GE_PY_PASS_PATH` 在运行时加载 `.py` 文件或目录 | 编译成 `.so` 后由 GE 加载 |
| Pattern 写法 | 推荐 `@pattern` 表达式写法，也兼容显式构图 | 使用 `EsGraphBuilder` 显式构图 |
| Replacement 写法 | 可返回表达式，例如 `return inputs[0]` | 返回 `GraphUniqPtr` |
| 适合场景 | 快速开发、业务侧按需接入 | 产品化交付、复用 C++ 代码、需要更强编译期控制 |

如果只是新增一个规则并验证效果，建议先用 Python 写。规则稳定后，如有交付形态或性能方面要求，再考虑 C++ 实现。

## 8. 开发前检查清单

写 pass 前先回答这些问题：

- 要处理的是“一段子图”还是“单个算子”？
- 如果是一段子图，pattern 的外部输入是否都声明了？
- 替换后仍会被外部使用的输出是否都声明了？
- 只是拓扑匹配就足够，还是需要在 `MeetRequirements` 里检查 dtype、shape、属性或 Const 值？
- replacement 的输入顺序是否和 pattern 边界一致？
- 注册阶段是否合适？如果在 InferShape 后运行，replacement 是否已经处理 shape 推导？
- 是否能通过 `DUMP_GE_GRAPH=1` 对比替换前后的图，确认规则确实生效？
