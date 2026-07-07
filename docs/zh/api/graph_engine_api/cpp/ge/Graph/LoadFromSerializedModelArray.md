# LoadFromSerializedModelArray

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

从ModelDef的序列化数据中恢复Graph。

## 函数原型

```c++
graphStatus LoadFromSerializedModelArray(const void *serialized_model, size_t size)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| serialized_model | 输入 | ModelDef序列化数据的指针。 |
| size | 输入 | ModelDef序列化数据的长度。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

仅支持读取air格式的文件。
