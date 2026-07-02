# Session

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.session import Session
from ge.error import GeError
```

## 功能说明

Session 类用于管理图的编译和执行会话。支持同步执行（run_graph）和异步执行
（run_graph_with_stream_async）。异步执行场景下，可通过 register_external_allocator 注册自定义内存分配器。
不支持拷贝和深拷贝。

## 类定义

```python
class Session:
    def __init__(self, options: Optional[dict] = None) -> None
    def add_graph(self, graph_id: int, graph: Graph, options: Optional[dict] = None) -> None
    def remove_graph(self, graph_id: int) -> None
    def run_graph(self, graph_id: int, inputs: List[Tensor]) -> List[Tensor]
    def run_graph_with_stream_async(self, graph_id: int, stream: int, inputs: List[Tensor]) -> List[Tensor]
    def register_external_allocator(self, stream: int, allocator: Allocator) -> None
    def unregister_external_allocator(self, stream: int) -> None
```

## 函数说明

### \_\_init\_\_

```python
def __init__(self, options: Optional[dict] = None) -> None
```

**功能说明**：创建会话实例，可传入配置字典进行初始化。若不传入配置，则使用默认配置创建会话。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| options | Optional[dict] | 可选 | 会话配置字典，键值对均为字符串类型。不传入时使用默认配置创建会话。 |

**返回值说明**：无返回值。

**约束说明**：
- options 必须为 dict 类型或 None，传入其他类型将抛出 TypeError。
- 会话创建失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
- Session 不支持拷贝（copy）和深拷贝（deepcopy），尝试拷贝将抛出 RuntimeError。

### add_graph

```python
def add_graph(self, graph_id: int, graph: Graph, options: Optional[dict] = None) -> None
```

**功能说明**：将图添加到会话中，支持传入额外的编译选项。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| graph_id | int | 必选 | 图的唯一标识，用于在会话中区分不同的图。 |
| graph | Graph | 必选 | 待添加的图对象。 |
| options | Optional[dict] | 可选 | 图编译配置字典，键值对均为字符串类型。不传入时使用默认配置。 |

**返回值说明**：无返回值。

**约束说明**：
- graph_id 必须为 int 类型，否则抛出 TypeError。
- graph 必须为 Graph 类型，否则抛出 TypeError。
- options 必须为 dict 类型或 None，否则抛出 TypeError。
- 添加图失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。

### remove_graph

```python
def remove_graph(self, graph_id: int) -> None
```

**功能说明**：从会话中移除指定图。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| graph_id | int | 必选 | 待移除图的唯一标识。 |

**返回值说明**：无返回值。

**约束说明**：
- graph_id 必须为 int 类型，否则抛出 TypeError。
- 移除图失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。

### run_graph

```python
def run_graph(self, graph_id: int, inputs: List[Tensor]) -> List[Tensor]
```

**功能说明**：同步执行指定图，传入输入张量列表，返回输出张量列表。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| graph_id | int | 必选 | 待执行图的唯一标识。 |
| inputs | List[Tensor] | 必选 | 输入张量列表，列表中所有元素必须为 Tensor 类型。 |

**返回值说明**：

| 返回值类型 | 说明 |
| :--------- | :--- |
| List[Tensor] | 图执行后的输出张量列表。 |

**约束说明**：
- graph_id 必须为 int 类型，否则抛出 TypeError。
- inputs 中所有元素必须为 Tensor 类型，否则抛出 TypeError。
- 图执行失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。

### run_graph_with_stream_async

```python
def run_graph_with_stream_async(self, graph_id: int, stream: int, inputs: List[Tensor]) -> List[Tensor]
```

**功能说明**：在指定 stream 上异步执行图，传入输入张量列表，返回输出张量列表。输出张量的内存分配优先使用
通过 register_external_allocator 注册的外部分配器；若未注册外部分配器，GE 将自动使用内置分配器。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| graph_id | int | 必选 | 待执行图的唯一标识。 |
| stream | int | 必选 | Stream 地址，用于指定异步执行的流。 |
| inputs | List[Tensor] | 必选 | 输入张量列表，列表中所有元素必须为 Tensor 类型。 |

**返回值说明**：

| 返回值类型 | 说明 |
| :--------- | :--- |
| List[Tensor] | 图执行后的输出张量列表。 |

**约束说明**：
- graph_id 必须为 int 类型，否则抛出 TypeError。
- stream 必须为 int 类型，否则抛出 TypeError。
- inputs 必须为 list 类型且所有元素必须为 Tensor 类型，否则抛出 TypeError。
- 若该 stream 未注册外部分配器且默认分配器注册失败，将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
- 图执行失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。

### register_external_allocator

```python
def register_external_allocator(self, stream: int, allocator: Allocator) -> None
```

**功能说明**：为指定 stream 注册外部内存分配器，用于管理异步执行场景下的设备内存分配。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| stream | int | 必选 | Stream 地址。 |
| allocator | Allocator | 必选 | 外部内存分配器实例，须为 Allocator 抽象基类的子类实例。 |

**返回值说明**：无返回值。

**约束说明**：
- stream 必须为 int 类型，否则抛出 TypeError。
- allocator 必须为 Allocator 实例，否则抛出 TypeError。
- 注册失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
- 同一 stream 重复注册会覆盖之前的外部分配器。

### unregister_external_allocator

```python
def unregister_external_allocator(self, stream: int) -> None
```

**功能说明**：注销指定 stream 上注册的外部内存分配器。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| stream | int | 必选 | Stream 地址。 |

**返回值说明**：无返回值。

**约束说明**：
- stream 必须为 int 类型，否则抛出 TypeError。
- 注销失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
