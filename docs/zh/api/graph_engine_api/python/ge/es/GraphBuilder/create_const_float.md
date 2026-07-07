# create\_const\_float

## 产品支持情况

全量芯片支持。

## 功能说明

创建float常量张量。

## 函数原型

```python
create_const_float(value: Union[float, List[float]], shape: Optional[List[int]] = None) -> TensorHolder
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| value | 输入 | 单个浮点数或浮点数列表。 |
| shape | 输入 | 可选参数，形状维度。 |

## 返回值说明

\(TensorHolder\)表示常量的张量持有者。

## 约束说明

- 如果是列表且提供了shape，元素数量必须与shape维度的乘积匹配。
- 如果value不是float、float列表或int类型，抛出TypeError。
- 如果值数量与形状不匹配，抛出ValueError。
- 如果常量创建失败，抛出RuntimeError。
