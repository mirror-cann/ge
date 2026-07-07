# GetInputConstData

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

如果指定算子Input对应的节点为Const节点，可调用该接口获取Const节点的数据。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus GetInputConstData(const std::string &dst_name, Tensor &data) const
graphStatus GetInputConstData(const char_t *dst_name, Tensor &data) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| dst_name | 输入 | 输入名称。 |
| data | 输出 | 返回Const节点的数据Tensor。 |

## 返回值说明

graphStatus类型：如果指定算子Input对应的节点为Const节点且获取数据成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
