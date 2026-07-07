# set\_input\_attr

## 产品支持情况

全量芯片支持。

## 功能说明

设置输入属性。

## 函数原型

```python
set_input_attr(attr_name: str, input_index: int, value: Any) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称，string类型。 |
| input_index | 输入 | 输入索引。 |
| value | 输入 | 属性值。可设置的属性值类型请参见[AttrValueType](l../../../../AttrValueType.md)。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果设置失败，抛出RuntimeError。
