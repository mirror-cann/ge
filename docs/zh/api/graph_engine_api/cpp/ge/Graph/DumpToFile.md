# DumpToFile

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

将graph序列化到执行路径下的文件中。

不包含权重等数据，只包含图结构及相关属性。

## 函数原型

```c++
graphStatus DumpToFile(DumpFormat format, const AscendString &suffix) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| format | 输入 | dump文件的格式，有onnx、txt等。DumpFormat枚举请参见[DumpFormat](../DumpFormat.md)。 |
| suffix | 输入 | 文件名称的前缀，比如ge_proto_xxxx_GraphPreRunBegin.txt中的GraphPreRunBegin为前缀 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
