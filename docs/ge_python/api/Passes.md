# Passes

## 产品支持情况

| 产品 | 是否支持 |
| :----------------------------------------------------------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.passes import (
    FusionBasePass,
    PatternFusionPass,
    DecomposePass,
    PassStage,
    pattern,
    register_fusion_pass,
    register_decompose_pass,
    create_pattern,
    create_replacement,
)
```

## 功能说明

Passes 模块提供 Python 级别的自定义图融合 Pass 开发框架。用户通过继承 `FusionBasePass`、`PatternFusionPass` 或 `DecomposePass` 来定义图优化 Pass，并通过注册装饰器将其注册到 GE 编译流程中。

- `FusionBasePass`：融合 Pass 基类，用户需要实现 `run()` 方法。
- `PatternFusionPass`：基于模式匹配的融合 Pass，继承自 `FusionBasePass`，用户可通过 `patterns()` 或 `@pattern` 方法定义模式，并实现 `replacement()`；`meet_requirements()` 为可选实现。其 `run()` 方法不会被引擎调用，不应重写。
- `DecomposePass`：算子分解 Pass，继承自 `FusionBasePass`，用户需要实现 `meet_requirements()` 和 `replacement()` 方法。其 `run()` 方法不会被引擎调用，不应重写。

---

## PassStage 枚举

Pass 执行阶段枚举，用于指定 Pass 在 GE 编译流程中的注册时机。

### 枚举值

| 枚举值 | 说明 |
| :----------------------------------------------------------- | :----------------------------------------------------------- |
| `BEFORE_INFER_SHAPE` | 推理形状之前 |
| `AFTER_INFER_SHAPE` | 推理形状之后 |
| `AFTER_ASSIGN_LOGIC_STREAM` | 分配逻辑流之后 |
| `AFTER_BUILTIN_FUSION_PASS` | 内置融合 Pass 之后 |
| `AFTER_ORIGIN_GRAPH_OPTIMIZE` | 原始图优化之后 |

---

## FusionBasePass 基类

所有自定义融合 Pass 的基类。

### 函数原型

```python
class FusionBasePass:
    def run(self, graph: Graph, context: PassContext) -> StatusLike:
        ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :------ | :-------- | :---- |
| graph | 输入 | 待优化的计算图对象，类型为 `ge.graph.Graph`。 |
| context | 输入 | Pass 执行上下文，类型为 `PassContext`，提供当前编译环境信息。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `StatusLike` | 返回 `None`、`bool` 或 `int`。返回 `None` 或真值表示执行成功，返回假值（`False` 或 `0`）表示执行失败。 |

---

## PatternFusionPass 基类

基于模式匹配的融合 Pass，继承自 `FusionBasePass`。执行引擎会调用 `patterns()`、`meet_requirements()` 和 `replacement()` 三个钩子方法，而非 `run()` 方法。

### 约束说明

- **不得重写 `run()` 方法**：如果子类中定义了 `run()` 方法，将在类定义时抛出 `TypeError`。
- 必须实现 `patterns()` 或至少一个 `@pattern` 方法，并实现 `replacement()` 方法；`meet_requirements()` 为可选实现（默认返回 `True`）。
- `@pattern` 方法不能和 `patterns()` 方法同时使用。
- 不支持 `patterns(self, inputs)` 写法；表达式式模式请使用 `@pattern` 方法。

### patterns() 方法

定义需要匹配的模式列表。该方法为 legacy 显式构图入口，适合直接返回一个或多个 `Pattern` / `Graph` 对象。

#### 函数原型

```python
def patterns(self) -> Iterable[PatternOrGraph]:
    ...
```

#### 参数说明

无参数。

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Iterable[PatternOrGraph]` | 返回一个可迭代对象，其中每个元素为 `Pattern` 或 `Graph` 类型，表示需要匹配的子图模式。 |

### @pattern 方法

使用 Python 表达式定义一个模式。一个 `@pattern` 方法对应一个 pattern，多个 pattern 可声明多个 `@pattern` 方法。

#### 函数原型

```python
@pattern
def add_zero(self, inputs):
    return inputs[0] + 0
```

#### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :----- | :-------- | :--- |
| inputs | 输入 | 表达式式 pattern 输入集合。`inputs[i]` 表示第 `i` 个图输入；`inputs[:N]` 用于显式声明连续的多个图输入。 |

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回单输出 pattern 表达式。 |
| `list[TensorHolder]` / `tuple[TensorHolder, ...]` | 返回一个多输出 pattern；该列表或元组不表示多个 pattern。 |

#### 约束说明

- `@pattern` 方法只负责声明 pattern，不接收 `match_result`。
- 多 pattern pass 通过多个 `@pattern` 方法声明，不通过一个方法返回多个 `Pattern` / `Graph`。
- `inputs` 的输入数量未知，因此不能直接迭代；多输入场景请使用 `inputs[:N]`。
- Python 层会自动创建 `GraphBuilder`、图输入、图输出，并自动 capture 已访问的 `inputs` 和 `@pattern` 返回的 pattern 输出。
- 自动 capture 顺序固定为：先按输入序号 capture 已访问的 `inputs`，再按 `return` 结构顺序 capture pattern 输出。同一个 Tensor 同时作为输入和输出时，会按这两个角色各 capture 一次。

### meet_requirements() 方法

判断匹配结果是否满足替换条件。

#### 函数原型

```python
def meet_requirements(self, match_result: MatchResult) -> bool:
    ...
```

#### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---------- | :-------- | :---- |
| match_result | 输入 | 模式匹配结果，类型为 `MatchResult`，包含匹配到的节点和边信息。 |

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `bool` | 返回 `True` 表示满足替换条件，将执行替换；返回 `False` 表示不满足，跳过本次替换。默认返回 `True`。 |

### replacement() 方法

生成替换子图。

#### 函数原型

```python
def replacement(self, match_result: MatchResult) -> Graph:
    ...
```

表达式式 pattern 也可使用以下写法：

```python
def replacement(self, inputs) -> TensorHolder:
    ...

def replacement(self, inputs, match_result) -> TensorHolder:
    ...
```

#### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---------- | :-------- | :---- |
| match_result | 输入 | 模式匹配结果，类型为 `MatchResult`，包含匹配到的节点和边信息。 |

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Graph` | 返回替换后的子图，类型为 `ge.graph.Graph`。 |

表达式式 `replacement(self, inputs)` 可返回 `TensorHolder` 或非空 `TensorHolder` 列表 / 元组，Python 层会自动构造替换图；需要读取匹配详情时可增加 `match_result` 参数。

---

## pattern 装饰器

标记 `PatternFusionPass` 的一个方法为表达式式 pattern 声明。

### 函数原型

```python
def pattern(method: Callable[..., object]) -> Callable[..., object]:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :----- | :-------- | :--- |
| method | 输入 | `PatternFusionPass` 子类中的实例方法，签名应为 `method(self, inputs)`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Callable[..., object]` | 返回原方法对象，并在类定义阶段由 `PatternFusionPass` 收集为 pattern。 |

### 示例

```python
from ge.passes import PatternFusionPass, pattern


class AlgebraicPass(PatternFusionPass):
    @pattern
    def add_zero(self, inputs):
        return inputs[0] + 0

    @pattern
    def mul_one(self, inputs):
        return inputs[0] * 1

    def replacement(self, inputs):
        return inputs[0]
```

---

## DecomposePass 基类

算子分解 Pass，继承自 `FusionBasePass`。执行引擎会对匹配到的节点调用 `meet_requirements()` 和 `replacement()` 方法，而非 `run()` 方法。

### 约束说明

- **不得重写 `run()` 方法**：如果子类中定义了 `run()` 方法，将在类定义时抛出 `TypeError`。
- 必须实现 `replacement()` 方法，`meet_requirements()` 为可选实现（默认返回 `True`）。
- 子类可定义类属性 `op_types: Optional[List[str]]`，用于指定需要分解的算子类型列表。使用 `register_decompose_pass` 装饰器时会自动设置该属性。

### meet_requirements() 方法

判断节点是否需要分解。

#### 函数原型

```python
def meet_requirements(self, node: Node) -> bool:
    ...
```

#### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| node | 输入 | 待判断的节点，类型为 `ge.graph.Node`。 |

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `bool` | 返回 `True` 表示需要分解，将执行替换；返回 `False` 表示不需要分解，跳过。默认返回 `True`。 |

### replacement() 方法

生成分解子图。

#### 函数原型

```python
def replacement(self, node: Node) -> Graph:
    ...
```

#### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| node | 输入 | 待分解的节点，类型为 `ge.graph.Node`。 |

#### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Graph` | 返回分解后的子图，类型为 `ge.graph.Graph`。 |

---

## register_fusion_pass 装饰器

注册融合 Pass 的类装饰器，用于将 `FusionBasePass` 或 `PatternFusionPass` 子类注册到 GE 编译流程中。

### 函数原型

```python
def register_fusion_pass(*, name: str, stage: PassStage, kind: Optional[str] = None) -> callable:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :----- | :-------- | :---- |
| name | 输入 | Pass 名称，字符串类型，必须唯一，不可与已注册的 Pass 名称重复。 |
| stage | 输入 | Pass 执行阶段，类型为 `PassStage` 枚举。 |
| kind | 输入 | Pass 类型标识，可选参数。若不指定，当被装饰类为 `PatternFusionPass` 子类时自动设为 `"pattern_fusion"`，否则设为 `"fusion_base"`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `callable` | 返回类装饰器函数，被装饰的类会被注册到 Pass 注册表中，并附加 `__ge_pass_descriptor__` 属性。 |

---

## register_decompose_pass 装饰器

注册分解 Pass 的类装饰器，用于将 `DecomposePass` 子类注册到 GE 编译流程中。

### 函数原型

```python
def register_decompose_pass(*, name: str, stage: PassStage, op_types: Iterable[str]) -> callable:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :------- | :-------- | :---- |
| name | 输入 | Pass 名称，字符串类型，必须唯一，不可与已注册的 Pass 名称重复。 |
| stage | 输入 | Pass 执行阶段，类型为 `PassStage` 枚举。 |
| op_types | 输入 | 需要分解的算子类型列表，类型为字符串的可迭代对象，不可为空，且每个元素必须为非空字符串。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `callable` | 返回类装饰器函数，被装饰的类会被注册到 Pass 注册表中，同时将 `op_types` 设置为类的属性。 |

---

## create_pattern 函数

从模式图构建原生 Pattern 对象。

### 函数原型

```python
def create_pattern(graph: Graph) -> Pattern:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| graph | 输入 | 模式图，类型为 `ge.graph.Graph`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Pattern` | 返回构建好的原生 `Pattern` 对象。 |

---

## Pattern.capture_tensor 方法

在模式图中标记需要捕获的 Tensor。被捕获的 Tensor 会按调用顺序保存，后续可通过 `match_result.get_captured_tensor(index)` 读取。

### 函数原型

```python
class Pattern:
    def capture_tensor(self, source: Union[TensorHolder, Node, NodeIo], index: Optional[int] = None) -> Pattern:
        ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :----- | :-------- | :---- |
| source | 输入 | 需要捕获的 Tensor 来源。支持 `TensorHolder`、`Node` 或 `NodeIo`。 |
| index | 输入 | 输出索引。当 `source` 为 `TensorHolder` 或 `Node` 时可指定输出索引，未指定时默认为 `0`；当 `source` 为 `NodeIo` 时不需要传入。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Pattern` | 返回当前 `Pattern`，支持链式调用。 |

### 示例

```python
pat = create_pattern(builder.build_and_reset([add]))
pat.capture_tensor(matmul)
pat.capture_tensor(add)
```

---

## create_replacement 函数

创建替换图，用于在模式融合或算子分解中提供替换子图。

### 函数原型

```python
def create_replacement(graph: Graph) -> Graph:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| graph | 输入 | 替换图，类型为 `ge.graph.Graph`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Graph` | 返回传入的替换图对象。若输入不是 `ge.graph.Graph` 类型，将抛出 `TypeError`。 |

---

## get_registered_passes 函数

获取所有已注册 Pass 的描述符列表。

### 函数原型

```python
def get_registered_passes() -> List[PassDescriptor]:
    ...
```

### 参数说明

无参数。

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `List[PassDescriptor]` | 返回已注册的 `PassDescriptor` 对象列表。 |

---

## get_registered_pass_dicts 函数

获取所有已注册 Pass 的字典表示列表。

### 函数原型

```python
def get_registered_pass_dicts() -> List[dict]:
    ...
```

### 参数说明

无参数。

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `List[dict]` | 返回已注册 Pass 的字典列表，每个字典包含 `descriptor_key`、`pass_name`、`module_name`、`class_name`、`stage`、`kind`、`op_types` 等字段。 |

---

## get_registered_pass_by_descriptor_key 函数

根据描述符键获取已注册的 Pass 描述符。

### 函数原型

```python
def get_registered_pass_by_descriptor_key(descriptor_key: str) -> Optional[PassDescriptor]:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :------------- | :-------- | :---- |
| descriptor_key | 输入 | Pass 描述符键，字符串类型，格式为 `{module_name}:{class_name}:{pass_name}`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `Optional[PassDescriptor]` | 返回匹配的 `PassDescriptor` 对象；若未找到则返回 `None`。 |

---

## clear_registered_passes 函数

清除所有已注册的 Pass。

### 函数原型

```python
def clear_registered_passes() -> None:
    ...
```

### 参数说明

无参数。

### 返回值说明

无返回值。

### 约束说明

- 此操作会清空整个 Pass 注册表，清除后所有已注册的 Pass 将不再可用。
