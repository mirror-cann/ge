# create\_variable

## 产品支持情况

全量芯片支持。

## 功能说明

创建变量张量。

## 函数原型

```python
create_variable(index: int, name: str) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 变量索引。 |
| name | 输入 | 变量名称。 |

## 返回值说明

\(TensorHolder\)表示变量的张量持有者。

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果变量创建失败，抛出RuntimeError。
