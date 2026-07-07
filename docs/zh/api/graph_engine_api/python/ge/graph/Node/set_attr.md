# set\_attr

## 产品支持情况

全量芯片支持。

## 功能说明

设置节点属性。

## 函数原型

```python
set_attr(key: str, value: Any) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| key | 输入 | 属性名称，string类型。 |
| value | 输入 | 属性值。可设置的属性值类型请参见[AttrValueType](../../AttrValueType.md)。 |

## 返回值说明

无

## 约束说明

- 如果参数类型不正确，抛出TypeError。
- 如果设置失败，抛出RuntimeError。
