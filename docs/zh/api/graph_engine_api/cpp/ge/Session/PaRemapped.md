# PaRemapped

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

用于内存出现UCE（Uncorrectable Error，不可纠正错误）错误时判断此段内存是否可以快速恢复。

## 函数原型

```c++
Status PaRemapped(const uint64_t va, const uint64_t new_pa, const uint64_t len)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| va | 输入 | 发生UCE错误的virtual address。 |
| new_pa | 输入 | 新申请内存的physical address。预留参数。 |
| len | 输入 | 发生UCE错误的内存大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：对应的va内存段可快速恢复。<br>FAILED：对应的VA内存段存在不可恢复的地址。<br>PARAM_INVALID：存在未识别到VA地址段。 |

## 约束说明

- 在调用本接口前，必须先调用[CompileGraph](CompileGraph.md)接口进行图编译。
- 不支持Session中存在Host调度的场景，若Session中存在Host调度图，接口返回失败。
- 不支持静态shape图allocation表中的绝对地址段。
- 该接口只适用于静态编译图，可以通过[GetCompiledGraphSummary](GetCompiledGraphSummary.md)接口中的IsStatic接口获取图是否为静态编译。
