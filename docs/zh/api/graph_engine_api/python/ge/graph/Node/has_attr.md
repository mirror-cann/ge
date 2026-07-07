# has\_attr

## 产品支持情况

全量芯片支持。

## 功能说明

检查节点是否具有指定属性。

## 函数原型

```python
has_attr(attr_name: str) -> bool
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name | 输入 | 属性名称，string类型。 |

## 返回值说明

\(bool\) 是否存在该属性。

## 约束说明

如果attr\_name不是字符串，抛出TypeError。
