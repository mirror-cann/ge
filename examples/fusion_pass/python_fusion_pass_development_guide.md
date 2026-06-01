# Python 融合 Pass 开发指南

本文面向想用 Python 编写 GE 融合 pass 的开发者。建议先阅读语言无关的机制说明：[融合 Pattern Pass 机制](../../docs/architecture/features/fusion_pattern_pass.md)。

如果你已经理解“定义 pattern、匹配、过滤、replacement、重连”这条主线，可以直接按本文写代码。

## 1. 为什么优先考虑 Python

Python pass 和 C++ pass 使用同一套 GE 匹配替换机制，但 Python 更适合快速开发和运行时接入：

- **接入便利**：把 `.py` 文件或目录配置到 `ASCEND_GE_PY_PASS_PATH`，GE 编译阶段会运行时加载，不需要先把 pass 编译成 `.so`。
- **表达更短**：`@pattern` 可以用 Python 表达式描述 pattern，例如 `return inputs[0] + 0`。
- **replacement 更直观**：简单替换可以直接写 `return inputs[0]`，不用手动创建 replacement graph。
- **便于迭代**：修改 Python 文件后重新触发编译即可验证，适合先把规则跑通。

## 2. 最小示例：删除 Add(x, 0)

目标：把图中的 `Add(x, 0)` 替换成 `x`。

```text
x ----\
       Add ---- out      ==>      x ---- out
0 ----/
```

Python 写法如下：

```python
from math import fabs

from ge.graph.types import DataType
from ge.passes import PassStage, PatternFusionPass, pattern, register_fusion_pass


def _scalar_value(value):
    while isinstance(value, list):
        if len(value) != 1:
            return None
        value = value[0]
    return value


def _is_zero(tensor):
    value = _scalar_value(tensor.data)
    if value is None:
        return False
    if tensor.data_type == DataType.DT_FLOAT:
        return fabs(float(value)) < 1e-6
    if tensor.data_type == DataType.DT_DOUBLE:
        return fabs(float(value)) < 1e-15
    if tensor.data_type == DataType.DT_INT32:
        return int(value) == 0
    return False


@register_fusion_pass(name="PythonAddZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)
class PythonAddZeroPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    def meet_requirements(self, match_result):
        for node in match_result.get_matched_nodes():
            if node.type != "Const":
                continue
            return _is_zero(node.get_attr("value"))
        return False

    def replacement(self, inputs):
        return inputs[0]
```

这段代码里只有三件事：

1. `@pattern` 方法描述要找的结构：第 0 个外部输入加一个常量。
2. `meet_requirements` 检查命中的常量是否真的是 0。
3. `replacement` 返回第 0 个外部输入，相当于删除这次命中的 `Add`。

完整可运行样例见 [AddZeroPass Python 样例](pattern_base_pass/4_add_zero_pass/python/README.md)。

## 3. 写一个 PatternFusionPass 的步骤

### 3.1 引入接口

常用导入如下：

```python
from ge.passes import (
    PassStage,
    PatternFusionPass,
    pattern,
    register_fusion_pass,
)
```

如果要写 `DecomposePass`，还需要：

```python
from ge.passes import DecomposePass, register_decompose_pass
```

接口完整说明见 [Python Passes API](../../docs/ge_python/api/Passes.md)。

### 3.2 注册 pass

用 `@register_fusion_pass` 把类注册给 GE：

```python
@register_fusion_pass(name="MyPass", stage=PassStage.BEFORE_INFER_SHAPE)
class MyPass(PatternFusionPass):
    ...
```

`name` 必须唯一。`stage` 表示执行阶段。初次开发建议使用 `PassStage.BEFORE_INFER_SHAPE`，因为 replacement 后还能进入 GE 后续统一的 shape 推导流程。

### 3.3 用 @pattern 定义要匹配的结构

`@pattern` 方法接收一个 `inputs` 对象。它代表 pattern 的外部输入集合。

```python
@pattern
def add_zero(self, inputs):
    return inputs[0] + 0
```

这里的 `inputs[0]` 是第 0 个外部输入占位符，不是某个固定真实节点。匹配时，GE 会把真实图中连进这段结构的 Tensor 对应到它。

多输入场景可以这样写：

```python
@pattern
def matmul_add(self, inputs):
    a, b, c = inputs[:3]
    return MatMul(a, b) + c
```

注意：

- `inputs[i]` 会按需创建第 `i` 个输入。
- `inputs[:N]` 用于显式声明连续的多个输入。
- `@pattern` 会自动 capture 已访问的外部输入和返回的 pattern 输出。capture 顺序固定为：先按输入序号 capture 外部输入，再按 `return` 结构顺序 capture pattern 输出。以上例为例，`a/b/c` 会分别作为第 0/1/2 个 captured tensor，`MatMul(a, b) + c` 的输出会作为第 3 个 captured tensor 出现在 `match_result` 中。
- 不要直接遍历 `inputs`，因为输入个数不是预先固定的。
- 一个 `@pattern` 方法只表示一个 pattern。
- 多个拓扑要写多个 `@pattern` 方法。
- `@pattern` 不能和 `patterns(self)` 同时使用。

### 3.4 用 meet_requirements 做条件过滤（可选）

如果拓扑命中后还要检查 dtype、shape、属性、常量值，就实现 `meet_requirements`：

```python
def meet_requirements(self, match_result):
    for node in match_result.get_matched_nodes():
        if node.type == "Const":
            return _is_zero(node.get_attr("value"))
    return False
```

`match_result` 是这次匹配的结果。它可以拿到命中的真实节点，也可以拿到 pattern 中捕获的 Tensor。使用 `@pattern` 时，pattern 中已访问的外部输入会按输入序号自动 capture，`return` 的 pattern 输出会随后按返回顺序自动 capture；未作为 `return` 输出的中间 Tensor 不会自动 capture。

如果只要拓扑匹配成功就替换，可以不写这个方法；默认返回 `True`。

### 3.5 用 replacement 定义替换结构

最简单的 replacement 可以直接返回某个输入：

```python
def replacement(self, inputs):
    return inputs[0]
```

也可以用表达式创建新结构：

```python
def replacement(self, inputs):
    a, b, c = inputs[:3]
    return GEMM(a, b, c, 1.0, 1.0)
```

如果 replacement 需要读取匹配到的节点属性，可以多接收一个 `match_result` 参数：

```python
def replacement(self, inputs, match_result):
    a, b, c = inputs[:3]
    transpose_a = False
    transpose_b = False
    for node in match_result.get_matched_nodes():
        if node.type not in ("MatMul", "BatchMatMulV2"):
            continue
        try:
            transpose_a = bool(node.get_attr("transpose_x1"))
            transpose_b = bool(node.get_attr("transpose_x2"))
        except RuntimeError:
            pass
        break
    return GEMM(a, b, c, 1.0, 1.0, transpose_a, transpose_b)
```

## 4. 什么时候不用 @pattern

`@pattern` 适合大多数常见拓扑，但它有一个明确边界：它会自动 capture 已访问的外部输入和 `return` 的 pattern 输出，不会自动 capture 未作为输出返回的中间 Tensor。

如果 `meet_requirements` 或 `replacement` 需要读取未作为 `return` 输出的中间 Tensor，例如 `MatMul` 输出，就不要使用 `@pattern`。此时应显式创建 pattern graph，再调用 `Pattern.capture_tensor` 标记需要读取的中间 Tensor。若只需要读取 `return` 返回的最终输出，例如 `Add` 输出，继续使用 `@pattern` 即可。

这种写法更接近 C++：

```python
from ge.es.graph_builder import GraphBuilder
from ge.passes import create_pattern, create_replacement


def patterns(self):
    builder = GraphBuilder("pattern")
    a, b, c = builder.create_inputs(3)
    matmul = MatMul(a, b)
    add = matmul + c
    pat = create_pattern(builder.build_and_reset([add]))
    pat.capture_tensor(matmul)
    pat.capture_tensor(add)
    return [pat]


def replacement(self, match_result):
    builder = GraphBuilder("replacement")
    a, b, c = builder.create_inputs(3)
    gemm = GEMM(a, b, c, builder.create_scalar_float(1.0), builder.create_scalar_float(1.0))
    return create_replacement(builder.build_and_reset([gemm]))
```

如果只是表达 `Add(x, 0)`、`MatMul + Add` 这类规则，优先用 `@pattern`，代码更短也更贴近优化逻辑。

## 5. 捕获 Tensor

捕获 Tensor 的作用是：在 pattern 命中后，从 `match_result` 中按捕获顺序取回对应的真实 Tensor。

常见用途：

- 检查某个输出 Tensor 的 dtype 或 shape。
- 读取原节点属性，传给 replacement 中的新节点。
- 打印命中位置，确认 pass 是否命中预期节点。

参考 [capture tensor Python 样例](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)。

## 6. 更严格的匹配：PatternMatcherConfig

默认 matcher 主要检查拓扑和算子类型。如果想在匹配阶段就检查 Const 值，可以在构造函数中传入配置：

```python
from ge.passes import PatternMatcherConfigBuilder


class PythonAddZeroConstValueMatchPass(PatternFusionPass):
    def __init__(self):
        super().__init__(
            PatternMatcherConfigBuilder()
            .enable_const_value_match()
            .build()
        )

    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0.0

    def replacement(self, inputs):
        return inputs[0]
```

这个写法更短，但 Const 值匹配是严格匹配，不做浮点容差，也不做跨 dtype 归一化。如果你的判断需要容差或更复杂逻辑，放在 `meet_requirements` 中更稳妥。

参考 [PatternMatcherConfig Python 样例](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)。

## 7. 写 DecomposePass

如果目标是“看到某种单个算子后，把它拆成一组算子”，用 `DecomposePass`。

骨架如下：

```python
from ge.passes import DecomposePass, PassStage, register_decompose_pass


@register_decompose_pass(
    name="PythonMyDecomposePass",
    stage=PassStage.AFTER_INFER_SHAPE,
    op_types=["Conv2D"],
)
class PythonMyDecomposePass(DecomposePass):
    def meet_requirements(self, node):
        return node.get_attr("groups") != 1

    def replacement(self, node):
        # 返回用基础算子组成的替换 graph
        ...
```

`op_types` 决定 GE 会把哪些类型的节点交给这个 pass。`meet_requirements` 再判断其中哪些节点真的需要替换。

完整样例见 [DecomposePass Python 样例](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)。

## 8. 运行 Python pass

### 8.1 设置环境

先设置 CANN 环境变量：

```bash
source ${ASCEND_PATH}/set_env.sh
```

再告诉 GE 从哪里加载 Python pass：

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
```

也可以指向目录：

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/
```

多个路径用冒号分隔：

```bash
export ASCEND_GE_PY_PASS_PATH=/path/to/a.py:/path/to/pass_dir/
```

详细扫描规则见 [ASCEND_GE_PY_PASS_PATH](../../docs/ge_python/env/ASCEND_GE_PY_PASS_PATH.md)。

### 8.2 离线编译

离线场景建议使用 `pyatc` 触发编译。`pyatc` 和 `atc` 的命令行参数一致，但会在当前 Python 解释器进程中运行，便于加载 Python pass。

```bash
pyatc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
```

### 8.3 在线场景

在线场景中，在触发 GE 编译前设置 `ASCEND_GE_PY_PASS_PATH`。样例中通常通过 `torch_forward.py` 触发在线编译和执行。

## 9. 验证和排查

建议每次开发都打开图 dump：

```bash
export DUMP_GE_GRAPH=1
```

然后对比替换前后的 `.pbtxt`：

- `PreRunBegin`：pass 执行前。
- `RunCustomPass...`：自定义 pass 执行后。

如果没有命中，按这个顺序排查：

| 现象 | 可能原因 | 检查方式 |
|------|----------|----------|
| Python 文件没被加载 | `ASCEND_GE_PY_PASS_PATH` 没设置、路径不存在、后缀不是 `.py` | 先确认环境变量和路径 |
| 类已加载但 pass 不执行 | 没有使用注册装饰器，或注册阶段不对 | 检查 `@register_fusion_pass` / `@register_decompose_pass` |
| pattern 不命中 | 算子类型、输入个数或输出边界不一致 | 对比 dump 图中的真实拓扑 |
| 命中了但不替换 | `meet_requirements` 返回 `False` | 打印命中节点属性 |
| 替换后图异常 | replacement 输出没有覆盖外部消费者需要的 Tensor | 回到机制文档检查边界规则 |

需要更多日志时可设置：

```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1
export ASCEND_GLOBAL_LOG_LEVEL=0
```

使用 `pyatc` 时，还可以增加 `--log=debug`。

## 10. 推荐阅读顺序

1. [融合 Pattern Pass 机制](../../docs/architecture/features/fusion_pattern_pass.md)
2. [AddZeroPass Python 样例](pattern_base_pass/4_add_zero_pass/python/README.md)
3. [MatMul+Add Python 样例](pattern_base_pass/1_fuse_matmul_add_pass/python/README.md)
4. [capture tensor Python 样例](pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)
5. [PatternMatcherConfig Python 样例](pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)
6. [DecomposePass Python 样例](pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)
