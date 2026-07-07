# create\_scalar\_uint64

## 产品支持情况

全量芯片支持。

## 功能说明

创建uint64标量张量。

## 函数原型

```python
create_scalar_uint64(value: int) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| value | 输入 | 整数值（必须非负）。 |

## 返回值说明

\(TensorHolder\)表示标量的张量持有者。

## 约束说明

- 如果value不是整数，抛出TypeError。
- 如果value为负数，抛出ValueError。
- 如果标量创建失败，抛出RuntimeError。
