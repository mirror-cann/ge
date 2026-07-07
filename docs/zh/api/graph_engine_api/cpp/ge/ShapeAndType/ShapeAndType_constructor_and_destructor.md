# ShapeAndType构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 函数功能

ShapeAndType构造函数和析构函数。

## 函数原型

```c++
ShapeAndType()
~ShapeAndType()
ShapeAndType(const Shape &shape, DataType data_type)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| shape | 输入 | 需设置的shape。 |
| data_type | 输入 | 需设置的DataType。 |

## 返回值

ShapeAndType构造函数返回ShapeAndType类型的对象。

## 异常处理

无。

## 约束说明

无。
