# CompileGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

同步编译指定ID对应的Graph图。

该接口不包含模型所需内存资源管理功能，而是将这部分管理内存的工作开放给用户。您可以配合编译后Graph资源占用查询接口、内存的基地址刷新接口来使用，达到自行管理模型内存、获得更多灵活性的目的。

您可以在调用该接口后，调用[GetCompiledGraphSummary](./GetCompiledGraphSummary.md)获取图编译结果的概要信息（比如模型执行所需的内存资源大小及内存是否可刷新、复用等），根据查询到的内存大小，自行申请并管理内存；然后通过[SetGraphConstMemoryBase](./SetGraphConstMemoryBase.md)、  [UpdateGraphFeatureMemoryBase](./UpdateGraphFeatureMemoryBase.md)对内存基址进行设置和刷新。

## 函数原型

```c++
Status CompileGraph(uint32_t graph_id)
Status CompileGraph(uint32_t graph_id, const std::vector<ge::Tensor> &inputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| inputs | 输入 | 用于更新计算图输入节点的元信息（Shape，Format，DataType）。可以为空。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：图编译成功。<br>FAILED：图编译失败。<br>PARAM_INVALID：图编译时，输入graph信息异常。 |
