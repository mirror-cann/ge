# Shape

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.graph import Shape
```

## 功能说明

Shape 类继承自 Python 内置的 `list`，用于表示张量的形状维度信息。除了具备标准列表的所有操作能力外，还提供了计算形状总元素数和判断是否为未知形状的便捷方法。当 dims 为 None 时表示标量（空列表）。

Shape 模块同时定义了以下常量：

| 常量名 | 值 | 说明 |
| :----- | :--- | :--- |
| UNKNOWN_DIM | -1 | 表示未知维度 |
| UNKNOWN_DIM_NUM | -2 | 表示未知维度数量 |
| UNKNOWN_DIM_SIZE | -1 | 未知形状时 get_shape_size() 的返回值 |

## 类定义

```python
class Shape(list):
    def __init__(self, dims: Optional[List[int]] = None) -> None
```

## 函数列表

| 函数 | 功能说明 |
| :--- | :--- |
| \_\_init\_\_(dims=None) | 构造函数，创建 Shape 对象。dims 为整数列表，None 表示标量（空列表） |
| get_shape_size() | 计算形状中所有维度的乘积，即张量的总元素数 |
| is_unknown_shape() | 判断形状中是否包含未知维度 |

## 参数说明

### \_\_init\_\_ 参数

| 参数名 | 类型 | 是否必选 | 说明 |
| :----- | :--- | :------: | :--- |
| dims | List[int] | 否 | 维度值列表，例如 [1, 3, 224, 224]。None 表示标量（空列表）。默认值为 None |

## 返回值说明

| 函数 | 返回值类型 | 说明 |
| :--- | :--- | :--- |
| get_shape_size() | int | 所有维度的乘积。当形状为空（标量）时返回 0；当形状中包含未知维度（UNKNOWN_DIM 或 UNKNOWN_DIM_NUM）时返回 -1 |
| is_unknown_shape() | bool | 如果形状中包含 UNKNOWN_DIM（-1）或 UNKNOWN_DIM_NUM（-2），返回 True；否则返回 False |

## 约束说明

- dims 参数必须为整数列表（list of int）或 None，否则抛出 TypeError。
- Shape 继承自 list，因此支持所有标准列表操作（索引、切片、迭代、len 等）。
- 当形状中包含未知维度时，get_shape_size() 返回 -1，而非抛出异常。

## 使用示例

```python
from ge.graph import Shape

# 创建 Shape 对象
shape = Shape([1, 3, 224, 224])

# 获取总元素数
print(shape.get_shape_size())  # 150528

# 判断是否为未知形状
print(shape.is_unknown_shape())  # False

# 创建包含未知维度的 Shape
unknown_shape = Shape([-1, 3, 224, 224])
print(unknown_shape.is_unknown_shape())  # True
print(unknown_shape.get_shape_size())    # -1

# 标量形状（空列表）
scalar = Shape()
print(len(scalar))              # 0
print(scalar.get_shape_size())  # 0

# 支持 list 操作
print(shape[0])      # 1
print(len(shape))    # 4
print(list(shape))   # [1, 3, 224, 224]
```
