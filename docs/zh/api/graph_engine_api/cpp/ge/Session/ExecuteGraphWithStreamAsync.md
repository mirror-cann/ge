# ExecuteGraphWithStreamAsync

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

异步运行指定ID对应的Graph图，输出执行结果。

此函数与[RunGraphWithStreamAsync](RunGraphWithStreamAsync.md)均用于运行指定ID对应的图并输出结果。与RunGraphWithStreamAsync不同的是：

- 该接口运行前需要完成[CompileGraph](CompileGraph.md)及[LoadGraph](LoadGraph.md)（异步运行Graph场景）流程。
- 该接口inputs和outputs数据类型为gert::Tensor。inputs和outputs均为Device上的内存空间，且需要在运行前由用户分配内存大小。

    如下两种情况如果不分配输出内存，ouputs传空：

  - 用户通过[RegisterExternalAllocator](RegisterExternalAllocator.md)设置了外置allocator，如果没有分配输出内存，由GE调用外置allocator的接口分配内存，用户需要在外置allocator析构前释放这块内存。
  - 用户没有设置外置allocator，GE将使用内置allocator分配内存，内存的生命周期与图的生命周期保持一致，用户需要在图卸载前（Session析构前或GEFinalize调用前）主动释放此内存，比如通过调用outputs.clear\(\)方法。若未及时释放，将引发未定义行为。

## 函数原型

```c++
Status ExecuteGraphWithStreamAsync(uint32_t graph_id, void *stream,const std::vector<gert::Tensor> &inputs,std::vector<gert::Tensor> &outputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要运行图对应的ID。 |
| stream | 输入 | 指定图在哪个Stream上运行。 |
| inputs | 输入 | 计算图输入Tensor，为Device上的内存空间。<br>如果通过options指定了ge.exec.hostInputIndexes参数，对应索引的Tensor可以为Host上的内存空间。 |
| outputs | 输出 | 计算图输出Tensor，为Device上的内存空间。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：异步运行图成功。<br>FAILED：异步运行图失败。 |

## 约束说明

- 调用该接口前，请先分配好Tensor需要使用的内存。
- 调用该接口前，需要完成[CompileGraph](CompileGraph.md)及[LoadGraph](LoadGraph.md)流程。
- 调用该接口前，需要通过acl提供的**aclrtCreateStream**接口创建Stream。
- 得到输出运行结果前，需要通过acl提供的**aclrtSynchronizeStream**接口保证Stream上的任务已经执行完。

    接口详细说明请参见《Runtime运行时 API》中的“Stream管理”。

## 调用示例

请参见[异步运行Graph](../../../../../user_guides/graph_dev/compile_and_run_graph/run_graph_asynchronously.md)。
