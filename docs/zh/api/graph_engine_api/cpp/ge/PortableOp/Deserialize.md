# Deserialize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：libgraph.so

## 功能说明

从buffer中读取序列化的二进制数据并反序列化恢复算子的kernel状态，与Serialize配合使用实现算子数据的持久化恢复，支持模型加载时重建算子运行环境。

## 函数原型

```c++
virtual graphStatus Deserialize(const std::vector<uint8_t> &buffer) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| buffer | 输入 | 输入的二进制数据。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
