# Serialize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

将自定义算子的kernel二进制数据序列化到buffer中，格式由算子自定义，GE不解析只透传，用于算子数据的持久化存储和恢复。

## 函数原型

```c++
virtual graphStatus Serialize(std::vector<uint8_t> &buffer) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| buffer | 输出 | 输出的二进制数据，由算子自定义格式，GE不解析只透传。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
