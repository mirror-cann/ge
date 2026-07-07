# set\_shape

## 产品支持情况

全量芯片支持。

## 功能说明

设置张量形状。

## 函数原型

```python
set_shape(shape: List[int]) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| shape | 输入 | 形状维度列表。 |

## 返回值说明

\(TensorHolder\)返回自身以支持链式调用。

## 约束说明

- 如果shape不是整数列表，抛出TypeError。
- 如果所有形状维度不是整数，抛出TypeError。
- 如果操作失败，抛出RuntimeError。
