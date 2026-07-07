# SetGraphConstMemoryBase

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

设置Graph的Const内存基址。

内存大小从[GetCompiledGraphSummary](GetCompiledGraphSummary.md)\>**GetConstMemorySize**接口中获取。

## 函数原型

```c++
Status SetGraphConstMemoryBase(uint32_t graph_id, const void *const memory, size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| memory | 输入 | 设置的Const内存基地址。 |
| size | 输入 | 设置的Const内存大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

- 在调用本接口前，必须先调用[CompileGraph](CompileGraph.md)接口进行图编译。
- 每个Graph只支持设置一次，不支持刷新。
- 该接口只适用于静态编译图，可以通过[GetCompiledGraphSummary](GetCompiledGraphSummary.md)接口中的**IsStatic**接口获取图是否为静态编译。
- 若使用了本接口，又配置了[RegisterExternalAllocator](RegisterExternalAllocator.md)接口，则RegisterExternalAllocator接口不生效。
