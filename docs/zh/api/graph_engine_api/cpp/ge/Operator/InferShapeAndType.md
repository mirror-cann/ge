# InferShapeAndType

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

推导Operator输出的shape和DataType。

关于DataType数据类型的定义，请参见[DataType](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。

## 函数原型

```c++
graphStatus InferShapeAndType()
```

## 参数说明

无。

## 返回值说明

graphStatus类型：推导成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无。

## 约束说明

无。
