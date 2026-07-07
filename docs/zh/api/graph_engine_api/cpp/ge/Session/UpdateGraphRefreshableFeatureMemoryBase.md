# UpdateGraphRefreshableFeatureMemoryBase

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

用于更新除了Fixed之外可刷新的Feature内存基址。

内存大小从[GetCompiledGraphSummary](GetCompiledGraphSummary.md)\>**GetRefreshableFeatureMemorySize**接口中获取。

如果图中存在Fixed内存，且用户没有调用[SetGraphFixedFeatureMemoryBase](SetGraphFixedFeatureMemoryBase.md)或[SetGraphFixedFeatureMemoryBaseWithType](SetGraphFixedFeatureMemoryBaseWithType.md)接口设置默认内存类型的Fixed Feature内存基地址，则会自动默认申请Fixed内存。

Feature内存指的是模型执行过程中所需要的中间内存（比如中间节点的输入输出等内存）。

## 函数原型

```c++
Status UpdateGraphRefreshableFeatureMemoryBase(uint32_t graph_id, const void *const memory, size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| memory | 输入 | 设置的除了Fixed之外可刷新的Feature内存基址。 |
| size | 输入 | 设置的除了Fixed之外可刷新的Feature内存大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

- 在调用本接口前，必须先调用[CompileGraph](CompileGraph.md)接口进行图编译。
- 调用[CompileGraph](CompileGraph.md)完成图编译后，您可以通过[GetCompiledGraphSummary](GetCompiledGraphSummary.md)接口获取Feature地址是否可刷新，只有Feature地址可刷新的图才支持重复调用本接口刷新Feature地址。
- 该接口只适用于静态编译图，可以通过[GetCompiledGraphSummary](GetCompiledGraphSummary.md)接口中的**IsStatic**接口获取图是否为静态编译。
- 若使用了本接口，又配置了[RegisterExternalAllocator](RegisterExternalAllocator.md)接口，则RegisterExternalAllocator接口不生效。
- 如果通过[SetGraphFixedFeatureMemoryBase](SetGraphFixedFeatureMemoryBase.md)或[SetGraphFixedFeatureMemoryBaseWithType](SetGraphFixedFeatureMemoryBaseWithType.md)接口将内存类型为MEMORY\_TYPE\_DEFAULT的Fixed内存地址设置为nullptr，size设置为0，表示关闭Fixed内存功能，该场景下无法再调用本接口。
- 不能与[UpdateGraphFeatureMemoryBase](UpdateGraphFeatureMemoryBase.md)同时使用。
