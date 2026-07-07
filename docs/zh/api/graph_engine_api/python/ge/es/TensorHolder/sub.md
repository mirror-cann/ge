# sub

## 产品支持情况

全量芯片支持。

## 功能说明

两个张量相减。

## 函数原型

```python
sub(other: Union[TensorHolder, TensorLike]) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| other | 输入 | 另一个张量持有者或类似张量的对象。 |

## 返回值说明

\(TensorHolder\)表示结果的新张量持有者。

## 约束说明

- 如果other不是TensorHolder或者TensorLike，抛出TypeError。
- 如果操作失败或库不可用，抛出RuntimeError。
- 支持-运算符重载。
