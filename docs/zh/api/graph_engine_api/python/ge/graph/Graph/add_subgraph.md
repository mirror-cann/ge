# add\_subgraph

## 产品支持情况

全量芯片支持。

## 功能说明

向图中添加子图。

## 函数原型

```python
add_subgraph(subgraph: Graph) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| subgraph | 输入 | 要添加的子图对象。 |

## 返回值说明

无

## 约束说明

- 如果subgraph不是Graph类型，抛出TypeError。
- 子图通过名称索引，父图中的子图名称必须唯一。
- 如果添加失败，抛出RuntimeError。
