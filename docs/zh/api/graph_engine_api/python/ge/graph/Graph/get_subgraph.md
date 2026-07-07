# get\_subgraph

## 产品支持情况

全量芯片支持。

## 功能说明

根据名称获取子图。

## 函数原型

```python
get_subgraph(name: str) -> Optional[Graph]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 子图名称，string类型。 |

## 返回值说明

\(Graph | None\)子图对象，未找到时返回None。

## 约束说明

- 如果name不是字符串，抛出TypeError。
- 如果获取失败，抛出RuntimeError。
