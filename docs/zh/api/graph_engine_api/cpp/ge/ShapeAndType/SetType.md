# SetType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 函数功能

设置ShapeAndType类的DataType。

## 函数原型

```c++
void SetType(DataType data_type)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| data_type | 输入 | 设置的目标DataType。类型请参见[DataType](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。 |

## 返回值

无。

## 异常处理

无。

## 约束说明

无。
