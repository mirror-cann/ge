# set\_graph\_attr\_bool

## 产品支持情况

全量芯片支持。

## 功能说明

为图设置bool属性。

## 函数原型

```python
set_graph_attr_bool(attr_name: str, value: bool) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称。 |
| value | 输入 | 布尔值。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果设置失败，抛出RuntimeError。
