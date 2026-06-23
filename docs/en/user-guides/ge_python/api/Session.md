# Session

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.session import Session
from ge.error import GeError
```

## Functionality Description

Session class manages graph compilation and execution sessions. Supports synchronous execution (run_graph) and asynchronous execution
(run_graph_with_stream_async). In asynchronous execution scenarios, custom memory allocator can be registered through register_external_allocator.
Does not support copy and deep copy.

## Class Definition

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

## Function Description

### \_\_init\_\_

```python
def __init__(self, options: Optional[dict] = None) -> None
```

**Functionality Description**: Creates session instance, can pass configuration dictionary for initialization. If no configuration passed, uses default configuration to create session.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| options | Optional[dict] | Optional | Session configuration dictionary, key-value pairs are string type. When not passed, uses default configuration to create session. |

**Return Value Description**: No return value.

**Constraint Description**:
- options must be dict type or None, passing other types will throw TypeError.
- When session creation fails, will throw GeError, exception information contains GE internal error information and interface context.
- Session does not support copy and deep copy, attempting to copy will throw RuntimeError.

### add_graph

```python
def add_graph(self, graph_id: int, graph: Graph, options: Optional[dict] = None) -> None
```

**Functionality Description**: Adds graph to session, supports passing additional compilation options.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| graph_id | int | Required | Unique identifier of graph, used to distinguish different graphs in session. |
| graph | Graph | Required | Graph object to be added. |
| options | Optional[dict] | Optional | Graph compilation configuration dictionary, key-value pairs are string type. When not passed, uses default configuration. |

**Return Value Description**: No return value.

**Constraint Description**:
- graph_id must be int type, otherwise throws TypeError.
- graph must be Graph type, otherwise throws TypeError.
- options must be dict type or None, otherwise throws TypeError.
- When adding graph fails, will throw GeError, exception information contains GE internal error information and interface context.

### remove_graph

```python
def remove_graph(self, graph_id: int) -> None
```

**Functionality Description**: Removes specified graph from session.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| graph_id | int | Required | Unique identifier of graph to be removed. |

**Return Value Description**: No return value.

**Constraint Description**:
- graph_id must be int type, otherwise throws TypeError.**Constraint Description**:
- graph_id must be int type, otherwise throws TypeError.
- When graph removal fails, throws GeError, exception message includes GE internal error message and interface context.

### run_graph

```python
def run_graph(self, graph_id: int, inputs: List[Tensor]) -> List[Tensor]
```

**Functionality Description**: Synchronously execute specified graph, pass input tensor list, return output tensor list.

**Parameter Description**:

| Parameter Name | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| graph_id | int | Required | Unique identifier of graph to execute. |
| inputs | List[Tensor] | Required | Input tensor list, all elements in list must be Tensor type. |

**Return Value Description**:

| Return Value Type | Description |
| :--------- | :--- |
| List[Tensor] | Output tensor list after graph execution. |

**Constraint Description**:
- graph_id must be int type, otherwise throws TypeError.
- All elements in inputs must be Tensor type, otherwise throws TypeError.
- When graph execution fails, throws GeError, exception message includes GE internal error message and interface context.

### run_graph_with_stream_async

```python
def run_graph_with_stream_async(self, graph_id: int, stream: int, inputs: List[Tensor]) -> List[Tensor]
```

**Functionality Description**: Asynchronously execute graph on specified stream, pass input tensor list, return output tensor list. Output tensor memory allocation prioritizes using external allocator registered through register_external_allocator; if no external allocator registered, GE will automatically use built-in allocator.

**Parameter Description**:

| Parameter Name | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| graph_id | int | Required | Unique identifier of graph to execute. |
| stream | int | Required | Stream address, used to specify stream for asynchronous execution. |
| inputs | List[Tensor] | Required | Input tensor list, all elements in list must be Tensor type. |

**Return Value Description**:

| Return Value Type | Description |
| :--------- | :--- |
| List[Tensor] | Output tensor list after graph execution. |

**Constraint Description**:
- graph_id must be int type, otherwise throws TypeError.
- stream must be int type, otherwise throws TypeError.
- inputs must be list type and all elements must be Tensor type, otherwise throws TypeError.
- If this stream has no registered external allocator and default allocator registration fails, throws GeError, exception message includes GE internal error message and interface context.
- When graph execution fails, throws GeError, exception message includes GE internal error message and interface context.

### register_external_allocator

```python
def register_external_allocator(self, stream: int, allocator: Allocator) -> None
```

**Functionality Description**: Register external memory allocator for specified stream, used to manage device memory allocation in asynchronous execution scenarios.

**Parameter Description**:

| Parameter Name | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| stream | int | Required | Stream address. |
| allocator | Allocator | Required | External memory allocator instance, must be subclass instance of Allocator abstract base class. |

**Return Value Description**: No return value.

**Constraint Description**:
- stream must be int type, otherwise throws TypeError.
- allocator must be Allocator instance, otherwise throws TypeError.
- When registration fails, throws GeError, exception message includes GE internal error message and interface context.
- Re-registering same stream will overwrite previous external allocator.

### unregister_external_allocator

```python
def unregister_external_allocator(self, stream: int) -> None
```

**Functionality Description**: Unregister external memory allocator registered on specified stream.

**Parameter Description**:

| Parameter Name | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| stream | int | Required | Stream address. |

**Return Value Description**: No return value.

**Constraint Description**:
- stream must be int type, otherwise throws TypeError.
- When unregistration fails, throws GeError, exception message includes GE internal error message and interface context.