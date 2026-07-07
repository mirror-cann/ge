# GetInputType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取输入定义的类型，返回之前通过[InputType](InputType.md)设置的输入IR类型（必需/可选/动态）。

## 函数原型

```c++
IrInputType GetInputType() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrInputType | 支持的枚举类型取值请参见[IrInputType](../IrInputType.md)。 |

## 约束说明

无
