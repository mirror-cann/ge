# create\_scalar\_float

## 产品支持情况

全量芯片支持。

## 功能说明

创建float标量张量。

## 函数原型

```python
create_scalar_float(value: float) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| value | 输入 | 浮点数值。 |

## 返回值说明

\(TensorHolder\)表示标量的张量持有者。

## 约束说明

- 如果value不是数字，抛出TypeError。
- 如果标量创建失败，抛出RuntimeError。
