# get\_attr

## 产品支持情况

全量芯片支持。

## 功能说明

获取节点属性。

## 函数原型

```python
get_attr(key: str) -> Any
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| key | 输入 | 属性名称，string类型。 |

## 返回值说明

属性值。

## 约束说明

- 如果key不是字符串，抛出TypeError。
- 如果获取失败，抛出RuntimeError。
