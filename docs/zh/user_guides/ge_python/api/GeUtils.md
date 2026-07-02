# GeUtils

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.utils import GeUtils
```

## 功能说明

`GeUtils` 提供 GE（Graph Engine）公共工具接口，包含 Shape 推导和 AICore 算子支持检查功能。

- `infer_shape`：对计算图进行 Shape 推导。该接口仅执行 Shape 推导，不进行其他图优化（如常量折叠、死边消除等）。
- `check_node_support_on_aicore`：检查节点是否在 AICore 上支持，返回是否支持的布尔值和不支持原因字符串。

## 类

### GeUtils

`GeUtils` 类仅包含静态方法，无需实例化即可使用。

| 静态方法 | 说明 |
| :--- | :--- |
| infer_shape | 对计算图进行 Shape 推导 |
| check_node_support_on_aicore | 检查节点是否在 AICore 上支持 |

## 函数原型

### infer_shape

```python
@staticmethod
def infer_shape(graph: Graph, input_shapes: List[List[int]]) -> None
```

对计算图进行 Shape 推导。仅执行 Shape 推导，不进行常量折叠、死边消除等其他图优化。

### check_node_support_on_aicore

```python
@staticmethod
def check_node_support_on_aicore(node: Node) -> Tuple[bool, str]
```

检查节点是否在 AICore 上支持，返回是否支持的布尔值和不支持原因。

## 参数说明

### infer_shape

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| graph | Graph | 是 | 待进行 Shape 推导的计算图对象 |
| input_shapes | List[List[int]] | 是 | 输入形状列表。列表中每个元素描述一个图输入的形状，每个形状为整数维度列表 |

### check_node_support_on_aicore

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| node | Node | 是 | 待检查的计算图节点对象 |

## 返回值说明

### infer_shape

无返回值。Shape 推导结果直接更新到图对象中。

### check_node_support_on_aicore

| 返回值 | 类型 | 说明 |
| :--- | :--- | :--- |
| is_supported | bool | 节点是否在 AICore 上支持。True 表示支持，False 表示不支持 |
| unsupported_reason | str | 不支持的原因描述。若节点支持，返回空字符串 |

## 约束说明

- `infer_shape` 仅执行 Shape 推导，不进行常量折叠、死边消除等其他图优化操作。
- `infer_shape` 的 `input_shapes` 必须为列表的列表，每个子列表中的元素必须为整数类型。
- `check_node_support_on_aicore` 需要传入有效的 `Node` 对象。
- `GeUtils` 的所有方法均为静态方法，无需创建实例即可调用。

## 使用示例

```python
from ge.utils import GeUtils
from ge.graph import Graph, Node

# Shape 推导
graph = Graph("my_graph")
# ... 构建图 ...
GeUtils.infer_shape(graph, [[1, 3, 224, 224], [1, 3, 224, 224]])

# 检查节点是否在 AICore 上支持
node = graph.get_node("node_name")
is_supported, reason = GeUtils.check_node_support_on_aicore(node)
if is_supported:
    print("节点在 AICore 上支持")
else:
    print(f"节点在 AICore 上不支持，原因：{reason}")
```
