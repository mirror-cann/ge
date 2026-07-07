# SetGraphFixedFeatureMemoryBase

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

用于指定图的Fixed Feature内存基地址。

内存大小从[GetCompiledGraphSummary](GetCompiledGraphSummary.md)\>**GetFixedFeatureMemorySize**接口中获取。

Feature内存指的是模型执行过程中所需要的中间内存（比如中间节点的输入输出等内存）

- 当图中包含Fixed内存，但是用户未指定时，默认申请Fixed内存。
- 用户可以通过将地址设置为nullptr，size设置为0的方式，关闭默认申请Fixed内存的行为。

## 函数原型

```c++
Status SetGraphFixedFeatureMemoryBase(uint32_t graph_id, const void * const memory, size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| memory | 输入 | 设置的Fixed Feature内存基地址。 |
| size | 输入 | 设置的Fixed Feature内存大小。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：设置成功。<br>FAILED：设置失败。 |

## 约束说明

- 在调用本接口前，必须先调用[CompileGraph](CompileGraph.md)接口进行图编译。
- 每个Graph只支持设置一次，不支持刷新。
- 不能与[UpdateGraphFeatureMemoryBase](UpdateGraphFeatureMemoryBase.md)接口同时使用。
- 不能与[SetGraphFixedFeatureMemoryBaseWithType](SetGraphFixedFeatureMemoryBaseWithType.md)接口同时使用，SetGraphFixedFeatureMemoryBaseWithType用来设置**不同内存类型**的Fixed内存基址；而SetGraphFixedFeatureMemoryBase用来设置Fixed内存基址和大小。
- 当Fixed内存长度为0时，调用该接口不生效，且会生成warning日志提示用户。
- [options参数说明](../options_params/options_parameters_description.md)\>内存管理中的ge.exec.staticMemoryPolicy参数设置为4或者GE\_USE\_STATIC\_MEMORY环境变量设置为4时，此时动静态图支持内存复用；而该接口不支持地址刷新，因此动静态图内存复用场景下不支持通过将地址设置为nullptr、size设置为0的方式，关闭默认申请Fixed内存的行为。

    环境变量详细说明请参见《环境变量参考》。
