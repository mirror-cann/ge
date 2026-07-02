# TensorHolder

## Product Support

| Product | Support |
| :----------------------------------------------------------- | :------: |
| Atlas A3 Training Products/Atlas A3 Inference Products | √ |
| Atlas A2 Training Products/Atlas A2 Inference Products | √ |

## Module Import

```python
from ge.es import TensorHolder
```

## Functionality Description

`TensorHolder` is tensor holder class in Eager-Style graph building, created by `GraphBuilder`'s `create_*` methods (like `create_input()`, `create_const_float()` etc.). This class supports operator overloading (`+`, `-`, `*`, `/`), can set data type, format and shape via chained calls.

`TensorHolder` automatically maintains strong reference to its owning `GraphBuilder`, ensuring underlying C++ resources are not released during `TensorHolder` lifetime.

### Constraints

- **Does not support direct instantiation**: `TensorHolder` objects can only be created via `GraphBuilder`'s `create_*` methods, directly calling constructor will throw `RuntimeError`.
- **Cannot call setter methods after `build_and_reset()`**: After `GraphBuilder.build_and_reset()` executes, calling `set_data_type()`, `set_format()`, `set_shape()` etc. setter methods will cause errors.
- **Operations require same GraphBuilder**: When performing tensor operations (`add`, `sub`, `mul`, `div`), both participating `TensorHolder` must belong to same `GraphBuilder`, otherwise will throw `ValueError`.

---

## name Property

Gets producer node name.

### Function Prototype

```python
@property
def name(self) -> str:
    ...
```

### Parameter Description

No parameters.

### Return Value Description

| Type | Description |
| :--- | :--- |
| `str` | Returns producer node's name. |

---

## set_data_type Method

Sets tensor data type.

### Function Prototype

```python
def set_data_type(self, data_type: DataType) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :-------- | :-------- | :---- |
| data_type | Input | Data type, type is `ge.graph.types.DataType` enum. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns current `TensorHolder` object reference, supports chained calls. |

### Constraints

- `data_type` must be `DataType` enum type, otherwise will throw `TypeError`.

---

## set_format Method

Sets tensor data format.

### Function Prototype

```python
def set_format(self, format: Format) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :----- | :-------- | :---- |
| format | Input | Data format, type is `ge.graph.types.Format` enum. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns current `TensorHolder` object reference, supports chained calls. |

### Constraints

- `format` must be `Format` enum type, otherwise will throw `TypeError`.

---

## set_shape Method

Sets tensor shape.

### Function Prototype

```python
def set_shape(self, shape: List[int]) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :---- | :-------- | :---- |
| shape | Input | Shape dimension list, type is integer list `List[int]`. |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns current `TensorHolder` object reference, supports chained calls. |

### Constraints

- `shape` must be integer list, and all elements must be `int` type, otherwise will throw `TypeError`.

---

## add Method

Tensor addition operation.

### Function Prototype

```python
def add(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :---- | :-------- | :---- |
| other | Input | Another tensor, type is `TensorHolder` or `TensorLike` (scalar/array etc. convertible types). |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns new `TensorHolder` object representing addition operation result. |

### Constraints

- If `other` is `TensorHolder`, must belong to same `GraphBuilder` as current tensor.
- Operation library (`libes_math.so` or default generated library) must be loadable, otherwise will throw `RuntimeError`.

---

## sub Method

Tensor subtraction operation.

### Function Prototype

```python
def sub(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :---- | :-------- | :---- |
| other | Input | Another tensor, type is `TensorHolder` or `TensorLike` (scalar/array etc. convertible types). |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns new `TensorHolder` object representing subtraction operation result. |

### Constraints

- If `other` is `TensorHolder`, must belong to same `GraphBuilder` as current tensor.
- Operation library (`libes_math.so` or default generated library) must be loadable, otherwise will throw `RuntimeError`.

---

## mul Method

Tensor multiplication operation.

### Function Prototype

```python
def mul(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :---- | :-------- | :---- |
| other | Input | Another tensor, type is `TensorHolder` or `TensorLike` (scalar/array etc. convertible types). |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns new `TensorHolder` object representing multiplication operation result. |

### Constraints

- If `other` is `TensorHolder`, must belong to same `GraphBuilder` as current tensor.
- Operation library (`libes_math.so` or default generated library) must be loadable, otherwise will throw `RuntimeError`.

---

## div Method

Tensor division operation.

### Function Prototype

```python
def div(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### Parameter Description

| Parameter | Input/Output | Description |
| :---- | :-------- | :---- |
| other | Input | Another tensor, type is `TensorHolder` or `TensorLike` (scalar/array etc. convertible types). |

### Return Value Description

| Type | Description |
| :--- | :--- |
| `TensorHolder` | Returns new `TensorHolder` object representing division operation result. |

### Constraints

- If `other` is `TensorHolder`, must belong to same `GraphBuilder` as current tensor.
- Operation library (`libes_math.so` or default generated library) must be loadable, otherwise will throw `RuntimeError`.

---

## Operator Overloading

`TensorHolder` supports the following Python operator overloading with corresponding relationships:

| Operator | Corresponding Method | Description |
| :----- | :------- | :--- |
| `a + b` | `a.add(b)` | Tensor addition |
| `a - b` | `a.sub(b)` | Tensor subtraction |
| `a * b` | `a.mul(b)` | Tensor multiplication |
| `a / b` | `a.div(b)` | Tensor division |

Also supports right operand operations (`__radd__`, `__rsub__`, `__rmul__`, `__rtruediv__`), for handling operations with non-`TensorHolder` type on left side.

---

## get_owner_builder Method

Gets owning `GraphBuilder`.

### Function Prototype

```python
def get_owner_builder(self) -> GraphBuilder:
    ...
```

### Parameter Description

No parameters.

### Return Value Description

| Type | Description |
| :--- | :--- |
| `GraphBuilder` | Returns `GraphBuilder` object that created this `TensorHolder`. |
