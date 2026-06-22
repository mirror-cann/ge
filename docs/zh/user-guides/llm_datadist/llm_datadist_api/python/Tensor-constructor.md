# Tensor-constructor

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品：支持
- Atlas A2 训练系列产品：不支持

## 函数功能

构造Tensor。

## 函数原型

```python
__init__(data, tensor_desc: TensorDesc = None)
```

## 参数说明

| 参数名 | 数据类型 | 取值说明 |
| --- | --- | --- |
| data | Union[np.ndarray, Tensor] | 表示Tensor的数据。 |
| tensor_desc | [TensorDesc](TensorDesc-constructor.md) | 表示Tensor的描述信息。 |

## 调用示例

```python
from llm_datadist import Tensor
tensor = Tensor(numpy.array([1]))
```

## 返回值

正确情况下返回Tensor的实例。

传入data信息和tensor\_desc信息不匹配时，会抛出RuntimeError。

## 约束说明

无
