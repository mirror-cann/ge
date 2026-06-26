# OfflineCompile

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.offline_compile import (
    build_initialize, build_finalize, build_model, save_model,
    bundle_build_model, bundle_save_model, GraphWithOptions, ModelBuffer
)
from ge.error import GeError
```

## Functionality Description

Offline compilation module provides the capability to compile computation graphs into offline models (`.om` files). Supports single graph compilation (`build_model`) and multi-graph bundle compilation (`bundle_build_model`).

Usage flow:

1. Call `build_initialize` to initialize offline compilation resources.
2. Call `build_model` to compile single graph, or call `bundle_build_model` to compile multiple graphs into bundle offline model.
3. Call `save_model` or `bundle_save_model` to save compilation result as `.om` file.
4. Call `build_finalize` to release offline compilation resources.

Configuration in `build_options` has higher priority than `global_options` in `build_initialize`.

When GE underlying interface execution fails, `build_initialize`, `build_model`, `save_model`, `bundle_build_model`,
`bundle_save_model` will throw `GeError`. `GeError` inherits from `RuntimeError`,
exception information contains GE internal error information and interface context.

## Data Classes

### GraphWithOptions

A combination of a graph and its corresponding compilation options, used in bundle compilation scenarios.

```python
@dataclass
class GraphWithOptions:
    graph: Graph
    build_options: Dict[str, str]
```

| Attribute | Type | Description |
| :--- | :--- | :--- |
| graph | Graph | Computation graph object to be compiled |
| build_options | Dict[str, str] | Compilation options dictionary corresponding to this graph, keys and values are strings, default is empty dictionary |

## Classes

### ModelBuffer

Offline model buffer class, used to store compiled offline model data. This class does not support direct instantiation, returned by `build_model` or `bundle_build_model`.

| Method/Attribute | Description |
| :--- | :--- |
| length (property) | Gets model buffer length (bytes), returns int |
| get_length() | Gets model buffer length (bytes), returns int |

## Function Prototypes

### build_initialize

```python
def build_initialize(global_options: Optional[dict] = None) -> None
```

Initializes resources required for offline compilation.

### build_finalize

```python
def build_finalize() -> None
```

Releases offline compilation resources.

### build_model

```python
def build_model(graph: Graph, build_options: Optional[dict] = None) -> ModelBuffer
```

Compiles single computation graph into offline model, model data saved in memory.

### save_model

```python
def save_model(output_file: str, model: ModelBuffer) -> None
```

Serializes offline model in memory into `.om` file.

### bundle_build_model

```python
def bundle_build_model(graph_with_options: List[GraphWithOptions]) -> ModelBuffer
```

Compiles multiple graphs into bundle offline model.

### bundle_save_model

```python
def bundle_save_model(output_file: str, model: ModelBuffer) -> None
```

Serializes bundle offline model in memory into `.om` file.

## Parameter Description

### build_initialize

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| global_options | Optional[dict] | No | Global compilation configuration dictionary, keys and values are strings. Default is None, means no additional configuration passed |

### build_model

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| graph | Graph | Yes | Computation graph object to be compiled |
| build_options | Optional[dict] | No | Graph-level compilation configuration dictionary, keys and values are strings. Configuration priority see constraint description. Default is None |

### save_model

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| output_file | str | Yes | Base name of output model file. Generated offline model file name automatically has `.om` suffix. If file name contains OS and architecture information, then this OM file can only be used in corresponding OS and architecture runtime environment |
| model | ModelBuffer | Yes | Offline model buffer object |

### bundle_build_model

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| graph_with_options | List[GraphWithOptions] | Yes | List containing multiple `GraphWithOptions`, each element contains a computation graph and its compilation options. List must contain 2 or more graphs |

### bundle_save_model

| Parameter | Type | Required | Description |
| :--- | :--- | :---: | :--- |
| output_file | str | Yes | Base name of output model file. Generated offline model file name automatically has `.om` suffix. If file name contains OS and architecture information, then this OM file can only be used in corresponding OS and architecture runtime environment |
| model | ModelBuffer | Yes | Bundle offline model buffer object |

## Return Value Description

### build_model

| Return Value | Type | Description |
| :--- | :--- | :--- |
| model | ModelBuffer | Buffer object containing compiled offline model data |

### bundle_build_model

| Return Value | Type | Description |
| :--- | :--- | :--- |
| model | ModelBuffer | Buffer object containing compiled bundle offline model data |

### Other Functions

`build_initialize`, `build_finalize`, `save_model`, `bundle_save_model` have no return value.

## Constraint Description

- Before using offline compilation module, must call `build_initialize` to initialize resources, after use must call `build_finalize` to release resources.
- Keys and values in `build_options` must be string type.
- `graph_with_options` list in `bundle_build_model` must contain 2 or more graphs.
- `ModelBuffer` objects do not support copy and deep copy, and do not support direct instantiation.
- `output_file` parameter in `save_model` and `bundle_save_model` is base name of output file, system automatically adds `.om` suffix.
- Configuration in `build_options` has higher priority than `global_options` in `build_initialize`.
- When GE underlying interface execution fails, will throw GeError, exception information contains GE internal error information and interface context.

## Usage Example

```python
from ge.offline_compile import (
    build_initialize, build_finalize,
    build_model, save_model,
    bundle_build_model, bundle_save_model,
    GraphWithOptions
)
from ge.graph import Graph

# Initialize offline compilation resources
build_initialize({"log_level": "0"})

# Single graph compilation
model = build_model(graph, {"input_format": "ND"})
save_model("output_model", model)
print(f"Model size: {model.length} bytes")

# Multi-graph bundle compilation
gwo_list = [
    GraphWithOptions(graph1, {"input_format": "ND"}),
    GraphWithOptions(graph2, {"input_format": "NCHW"}),
]
bundle_model = bundle_build_model(gwo_list)
bundle_save_model("bundle_output", bundle_model)

# Release offline compilation resources
build_finalize()
```
