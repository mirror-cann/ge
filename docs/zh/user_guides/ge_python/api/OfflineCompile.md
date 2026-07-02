# OfflineCompile

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.offline_compile import (
    build_initialize, build_finalize, build_model, save_model,
    bundle_build_model, bundle_save_model, GraphWithOptions, ModelBuffer
)
from ge.error import GeError
```

## 功能说明

离线编译模块提供将计算图编译为离线模型（`.om`文件）的能力。支持单图编译（`build_model`）和多图 bundle 编译（`bundle_build_model`）。

使用流程如下：

1. 调用 `build_initialize` 初始化离线编译资源。
2. 调用 `build_model` 编译单个图，或调用 `bundle_build_model` 编译多图为 bundle 离线模型。
3. 调用 `save_model` 或 `bundle_save_model` 将编译结果保存为 `.om` 文件。
4. 调用 `build_finalize` 释放离线编译资源。

`build_options` 中的配置优先级高于 `build_initialize` 中的 `global_options`。

当 GE 底层接口执行失败时，`build_initialize`、`build_model`、`save_model`、`bundle_build_model`、
`bundle_save_model` 将抛出 `GeError`。`GeError` 继承自 `RuntimeError`，
异常信息包含 GE 内部错误信息和接口上下文。

## 数据类

### GraphWithOptions

一个图与其对应的编译选项的组合，用于 bundle 编译场景。

```python
@dataclass
class GraphWithOptions:
    graph: Graph
    build_options: Dict[str, str]
```

| 属性 | 类型 | 说明 |
| :--- | :--- | :--- |
| graph | Graph | 待编译的计算图对象 |
| build_options | Dict[str, str] | 该图对应的编译选项字典，键和值均为字符串，默认为空字典 |

## 类

### ModelBuffer

离线模型缓冲区类，用于存储编译后的离线模型数据。该类不支持直接实例化，由 `build_model` 或 `bundle_build_model` 返回。

| 方法/属性 | 说明 |
| :--- | :--- |
| length（property） | 获取模型缓冲区长度（字节），返回 int |
| get_length() | 获取模型缓冲区长度（字节），返回 int |

## 函数原型

### build_initialize

```python
def build_initialize(global_options: Optional[dict] = None) -> None
```

初始化离线编译所需的资源。

### build_finalize

```python
def build_finalize() -> None
```

释放离线编译资源。

### build_model

```python
def build_model(graph: Graph, build_options: Optional[dict] = None) -> ModelBuffer
```

编译单个计算图为离线模型，模型数据保存在内存中。

### save_model

```python
def save_model(output_file: str, model: ModelBuffer) -> None
```

将内存中的离线模型序列化为 `.om` 文件。

### bundle_build_model

```python
def bundle_build_model(graph_with_options: List[GraphWithOptions]) -> ModelBuffer
```

编译多个图为 bundle 离线模型。

### bundle_save_model

```python
def bundle_save_model(output_file: str, model: ModelBuffer) -> None
```

将内存中的 bundle 离线模型序列化为 `.om` 文件。

## 参数说明

### build_initialize

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| global_options | Optional[dict] | 否 | 全局编译配置字典，键和值均为字符串。默认为 None，表示不传入额外配置 |

### build_model

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| graph | Graph | 是 | 待编译的计算图对象 |
| build_options | Optional[dict] | 否 | 图级别的编译配置字典，键和值均为字符串。配置优先级见约束说明。默认为 None |

### save_model

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| output_file | str | 是 | 输出模型文件的基础名称。生成的离线模型文件名自动以 `.om` 为后缀。若文件名中包含操作系统和架构信息，则该 OM 文件仅可在对应的操作系统和架构的运行环境中使用 |
| model | ModelBuffer | 是 | 离线模型缓冲区对象 |

### bundle_build_model

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| graph_with_options | List[GraphWithOptions] | 是 | 包含多个 `GraphWithOptions` 的列表，每个元素包含一个计算图及其编译选项。列表中必须包含 2 个及以上图 |

### bundle_save_model

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| output_file | str | 是 | 输出模型文件的基础名称。生成的离线模型文件名自动以 `.om` 为后缀。若文件名中包含操作系统和架构信息，则该 OM 文件仅可在对应的操作系统和架构的运行环境中使用 |
| model | ModelBuffer | 是 | bundle 离线模型缓冲区对象 |

## 返回值说明

### build_model

| 返回值 | 类型 | 说明 |
| :--- | :--- | :--- |
| model | ModelBuffer | 包含编译后离线模型数据的缓冲区对象 |

### bundle_build_model

| 返回值 | 类型 | 说明 |
| :--- | :--- | :--- |
| model | ModelBuffer | 包含编译后 bundle 离线模型数据的缓冲区对象 |

### 其他函数

`build_initialize`、`build_finalize`、`save_model`、`bundle_save_model` 无返回值。

## 约束说明

- 使用离线编译模块前，必须先调用 `build_initialize` 初始化资源，使用完毕后必须调用 `build_finalize` 释放资源。
- `build_options` 中键和值必须均为字符串类型。
- `bundle_build_model` 的 `graph_with_options` 列表中必须包含 2 个及以上图。
- `ModelBuffer` 对象不支持拷贝（copy）和深拷贝（deepcopy），也不支持直接实例化。
- `save_model` 和 `bundle_save_model` 的 `output_file` 参数为输出文件的基础名称，系统会自动添加 `.om` 后缀。
- `build_options` 中的配置优先级高于 `build_initialize` 中的 `global_options`。
- GE 底层接口执行失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。

## 使用示例

```python
from ge.offline_compile import (
    build_initialize, build_finalize,
    build_model, save_model,
    bundle_build_model, bundle_save_model,
    GraphWithOptions
)
from ge.graph import Graph

# 初始化离线编译资源
build_initialize({"log_level": "0"})

# 单图编译
model = build_model(graph, {"input_format": "ND"})
save_model("output_model", model)
print(f"模型大小：{model.length} 字节")

# 多图 bundle 编译
gwo_list = [
    GraphWithOptions(graph1, {"input_format": "ND"}),
    GraphWithOptions(graph2, {"input_format": "NCHW"}),
]
bundle_model = bundle_build_model(gwo_list)
bundle_save_model("bundle_output", bundle_model)

# 释放离线编译资源
build_finalize()
```
