# find\_node\_by\_name

## 产品支持情况

全量芯片支持。

## 功能说明

根据名称查找节点。

## 函数原型

```python
find_node_by_name(name: str) -> Node
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 节点名称。 |

## 返回值说明

找到的节点。

## 约束说明

- 如果name不是字符串，抛出TypeError。
- 如果查找失败，抛出RuntimeError。
