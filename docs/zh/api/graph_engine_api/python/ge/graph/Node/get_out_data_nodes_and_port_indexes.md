# get\_out\_data\_nodes\_and\_port\_indexes

## 产品支持情况

全量芯片支持。

## 功能说明

获取指定输出索引的数据节点和端口索引列表。

## 函数原型

```python
get_out_data_nodes_and_port_indexes(out_index: int) -> List[Tuple[Node, int]]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| out_index | 输入 | 输出索引。 |

## 返回值说明

\(List\[Tuple\[Node, int\]\]\) \(输出节点, 端口索引\) 列表。

## 约束说明

- 如果out\_index不是整数，抛出TypeError。
- 如果获取失败，抛出RuntimeError。
