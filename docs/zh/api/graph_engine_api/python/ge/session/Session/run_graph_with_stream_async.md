# run\_graph\_with\_stream\_async

## 功能说明

异步运行图。

## 函数原型

```python
run_graph_with_stream_async(graph_id: int, stream: int, inputs: List[Tensor]) -> List[Tensor]
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph_id | 输入 | 图ID。 |
| stream | 输入 | 指定图在哪个Stream上运行。 |
| inputs | 输入 | 输入张量列表，为Device上的内存空间。 |

## 返回值说明

\(List\[Tensor\]\)输出张量列表，为Device上的内存空间。

## 约束说明

- 如果graph\_id不是整数，抛出TypeError。
- 如果stream不是整数，抛出TypeError。
- 如果inputs中有元素不是Tensor类型，抛出TypeError。
- 如果运行失败，抛出RuntimeError。
- 调用该接口前，需要通过acl提供的**acl.rt.create\_stream\(\)**接口创建Stream，且只支持Stream为默认Context的场景；得到输出运行结果前，需要通过**acl.rt.synchronize\_stream**接口保证Stream上的任务已经执行完。接口详细说明请参见“[Stream管理](https://hiascend.com/document/redirect/CannCommunitycreatestream)”。
