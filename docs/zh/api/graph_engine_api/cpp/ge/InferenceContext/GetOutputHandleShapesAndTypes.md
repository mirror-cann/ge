# GetOutputHandleShapesAndTypes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

在推理上下文中，获取算子输出句柄的[ShapeAndType](../ShapeAndType/ShapeAndType.md)。

## 函数原型

```c++
const std::vector<std::vector<ShapeAndType>> &GetOutputHandleShapesAndTypes() const
```

## 参数说明

无。

## 返回值说明

算子输出句柄的[ShapeAndType](../ShapeAndType/ShapeAndType.md)。

## 异常处理

无。

## 约束说明

无。
