# GetCompiledGraphSummary

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

查询图编译结果的概要信息。包括Feature内存大小、Const内存大小、Stream、Event数目及内存是否可刷新等信息。

- 在调用本接口前，必须先调用[CompileGraph](./CompileGraph.md)接口进行图编译。
- 您可以自行申请内存，再通过如下接口设置或更新Feature内存、Const内存基址：
    - 通过[SetGraphConstMemoryBase](./SetGraphConstMemoryBase.md)设置Const内存基址。内存大小从**GetCompiledGraphSummary\>GetConstMemorySize**接口中获取。
    - 通过[UpdateGraphFeatureMemoryBase](./UpdateGraphFeatureMemoryBase.md)更新Feature内存基址，内存大小从**GetCompiledGraphSummary\>GetFeatureMemorySize**接口中获取。
    - 通过[SetGraphFixedFeatureMemoryBaseWithType](./SetGraphFixedFeatureMemoryBaseWithType.md)设置图的不同内存类型的Fixed Feature内存基地址，内存大小从**GetCompiledGraphSummary\>GetAllFeatureMemoryTypeSize**接口中获取。
    - 通过[UpdateGraphRefreshableFeatureMemoryBase](./UpdateGraphRefreshableFeatureMemoryBase.md)更新除了Fixed之外可刷新的Feature内存基址，内存大小从**GetCompiledGraphSummary\>GetRefreshableFeatureMemorySize**接口中获取。

## 函数原型

```c++
CompiledGraphSummaryPtr GetCompiledGraphSummary(uint32_t graph_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | CompiledGraphSummaryPtr | 图编译结果的概要信息[CompiledGraphSummary](../CompiledGraphSummary/CompiledGraphSummary.md)的shared_ptr。 |

## 约束说明

无
