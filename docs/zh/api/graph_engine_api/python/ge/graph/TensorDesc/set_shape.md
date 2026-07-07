# set\_shape

## 产品支持情况

全量芯片支持。

## 功能说明

设置TensorDesc的张量形状。

## 函数原型

```python
set_shape(shape: List[int]) -> TensorDesc
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| shape | 输入 | 新的张量形状，类型为list[int]，可传入Shape对象。 |

## 返回值说明

\(TensorDesc\) 返回自身以支持链式调用。

## 约束说明

如果shape不是整数列表，抛出TypeError。
