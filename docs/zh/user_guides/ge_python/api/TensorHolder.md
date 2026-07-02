# TensorHolder

## 产品支持情况

| 产品 | 是否支持 |
| :----------------------------------------------------------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.es import TensorHolder
```

## 功能说明

`TensorHolder` 是 Eager-Style 图构建中的张量持有者类，由 `GraphBuilder` 的 `create_*` 方法（如 `create_input()`、`create_const_float()` 等）创建。该类支持运算符重载（`+`、`-`、`*`、`/`），可通过链式调用设置数据类型、格式和形状。

`TensorHolder` 自动维护对所属 `GraphBuilder` 的强引用，确保底层 C++ 资源在 `TensorHolder` 存活期间不会被释放。

### 约束说明

- **不支持直接实例化**：`TensorHolder` 对象只能通过 `GraphBuilder` 的 `create_*` 方法创建，直接调用构造函数将抛出 `RuntimeError`。
- **不可在 `build_and_reset()` 后调用 setter 方法**：在 `GraphBuilder.build_and_reset()` 执行后，调用 `set_data_type()`、`set_format()`、`set_shape()` 等 setter 方法将导致错误。
- **运算要求同一 GraphBuilder**：进行张量运算（`add`、`sub`、`mul`、`div`）时，参与运算的两个 `TensorHolder` 必须属于同一个 `GraphBuilder`，否则将抛出 `ValueError`。

---

## name 属性（property）

获取生产者节点名称。

### 函数原型

```python
@property
def name(self) -> str:
    ...
```

### 参数说明

无参数。

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `str` | 返回生产者节点的名称。 |

---

## set_data_type 方法

设置张量数据类型。

### 函数原型

```python
def set_data_type(self, data_type: DataType) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :-------- | :-------- | :---- |
| data_type | 输入 | 数据类型，类型为 `ge.graph.types.DataType` 枚举。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回当前 `TensorHolder` 对象引用，支持链式调用。 |

### 约束说明

- `data_type` 必须为 `DataType` 枚举类型，否则将抛出 `TypeError`。

---

## set_format 方法

设置张量数据格式。

### 函数原型

```python
def set_format(self, format: Format) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :----- | :-------- | :---- |
| format | 输入 | 数据格式，类型为 `ge.graph.types.Format` 枚举。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回当前 `TensorHolder` 对象引用，支持链式调用。 |

### 约束说明

- `format` 必须为 `Format` 枚举类型，否则将抛出 `TypeError`。

---

## set_shape 方法

设置张量形状。

### 函数原型

```python
def set_shape(self, shape: List[int]) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| shape | 输入 | 形状维度列表，类型为整数列表 `List[int]`。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回当前 `TensorHolder` 对象引用，支持链式调用。 |

### 约束说明

- `shape` 必须为整数列表，且所有元素必须为 `int` 类型，否则将抛出 `TypeError`。

---

## add 方法

张量加法运算。

### 函数原型

```python
def add(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| other | 输入 | 另一个张量，类型为 `TensorHolder` 或 `TensorLike`（标量/数组等可转换类型）。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回新的 `TensorHolder` 对象，表示加法运算的结果。 |

### 约束说明

- 若 `other` 为 `TensorHolder`，必须与当前张量属于同一个 `GraphBuilder`。
- 运算库（`libes_math.so` 或默认生成的库）必须可加载，否则将抛出 `RuntimeError`。

---

## sub 方法

张量减法运算。

### 函数原型

```python
def sub(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| other | 输入 | 另一个张量，类型为 `TensorHolder` 或 `TensorLike`（标量/数组等可转换类型）。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回新的 `TensorHolder` 对象，表示减法运算的结果。 |

### 约束说明

- 若 `other` 为 `TensorHolder`，必须与当前张量属于同一个 `GraphBuilder`。
- 运算库（`libes_math.so` 或默认生成的库）必须可加载，否则将抛出 `RuntimeError`。

---

## mul 方法

张量乘法运算。

### 函数原型

```python
def mul(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| other | 输入 | 另一个张量，类型为 `TensorHolder` 或 `TensorLike`（标量/数组等可转换类型）。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回新的 `TensorHolder` 对象，表示乘法运算的结果。 |

### 约束说明

- 若 `other` 为 `TensorHolder`，必须与当前张量属于同一个 `GraphBuilder`。
- 运算库（`libes_math.so` 或默认生成的库）必须可加载，否则将抛出 `RuntimeError`。

---

## div 方法

张量除法运算。

### 函数原型

```python
def div(self, other: Union[TensorHolder, TensorLike]) -> TensorHolder:
    ...
```

### 参数说明

| 参数名 | 输入/输出 | 说明 |
| :---- | :-------- | :---- |
| other | 输入 | 另一个张量，类型为 `TensorHolder` 或 `TensorLike`（标量/数组等可转换类型）。 |

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `TensorHolder` | 返回新的 `TensorHolder` 对象，表示除法运算的结果。 |

### 约束说明

- 若 `other` 为 `TensorHolder`，必须与当前张量属于同一个 `GraphBuilder`。
- 运算库（`libes_math.so` 或默认生成的库）必须可加载，否则将抛出 `RuntimeError`。

---

## 运算符重载

`TensorHolder` 支持以下 Python 运算符重载，对应关系如下：

| 运算符 | 对应方法 | 说明 |
| :----- | :------- | :--- |
| `a + b` | `a.add(b)` | 张量加法 |
| `a - b` | `a.sub(b)` | 张量减法 |
| `a * b` | `a.mul(b)` | 张量乘法 |
| `a / b` | `a.div(b)` | 张量除法 |

同时支持右操作数运算（`__radd__`、`__rsub__`、`__rmul__`、`__rtruediv__`），用于处理非 `TensorHolder` 类型在左侧的运算。

---

## get_owner_builder 方法

获取所属的 `GraphBuilder`。

### 函数原型

```python
def get_owner_builder(self) -> GraphBuilder:
    ...
```

### 参数说明

无参数。

### 返回值说明

| 类型 | 说明 |
| :--- | :--- |
| `GraphBuilder` | 返回创建该 `TensorHolder` 的 `GraphBuilder` 对象。 |
