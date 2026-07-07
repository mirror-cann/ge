# SetInputHandleShapesAndTypes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

在推理上下文中，设置算子输入句柄的[ShapeAndType](../ShapeAndType/overview.md)。

## 函数原型

```c++
void SetInputHandleShapesAndTypes(std::vector<std::vector<ShapeAndType>> &&shapes_and_types)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| shapes_and_types | 输入 | 算子输入句柄的[ShapeAndType](../ShapeAndType/overview.md)。 |

## 返回值说明

无。

## 异常处理

无。

## 约束说明

无。
