# get\_in\_data\_nodes\_and\_port\_indexes

## 产品支持情况

全量芯片支持。

## 功能说明

获取指定输入索引的数据节点和端口索引。

## 函数原型

```python
get_in_data_nodes_and_port_indexes(in_index: int) -> Tuple[Node, int]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| in_index | 输入 | 输入索引。 |

## 返回值说明

\(Tuple\[Node, int\]\) \(输入节点, 端口索引\)。

## 约束说明

- 如果in\_index不是整数，抛出TypeError。
- 如果获取失败，抛出RuntimeError。
