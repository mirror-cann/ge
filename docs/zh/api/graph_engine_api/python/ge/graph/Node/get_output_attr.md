# get\_output\_attr

## 产品支持情况

全量芯片支持。

## 功能说明

获取输出属性。

## 函数原型

```python
get_output_attr(attr_name: str, output_index: int) -> Any
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称，string类型。 |
| output_index | 输入 | 输出索引。 |

## 返回值说明

属性值。

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果获取失败，抛出RuntimeError。
