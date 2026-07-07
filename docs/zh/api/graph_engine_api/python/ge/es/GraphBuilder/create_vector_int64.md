# create\_vector\_int64

## 产品支持情况

全量芯片支持。

## 功能说明

创建int64向量张量。

## 函数原型

```python
create_vector_int64(value: List[int]) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| value | 输入 | 整数列表。 |

## 返回值说明

\(TensorHolder\)表示向量的张量持有者。

## 约束说明

- 如果value不是整数列表，抛出TypeError。
- 如果向量创建失败，抛出RuntimeError。
