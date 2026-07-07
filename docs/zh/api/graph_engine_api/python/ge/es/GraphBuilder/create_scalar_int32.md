# create\_scalar\_int32

## 产品支持情况

全量芯片支持。

## 功能说明

创建int32标量张量。

## 函数原型

```python
create_scalar_int32(value: int) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| value | 输入 | 整数值。 |

## 返回值说明

\(TensorHolder\)表示标量的张量持有者。

## 约束说明

- 如果value不是整数，抛出TypeError。
- 如果标量创建失败，抛出RuntimeError。
