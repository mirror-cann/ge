# OutputType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输出类型（必需/动态）。

## 函数原型

```c++
IrOutputDefV2 &OutputType(IrOutputType ir_output_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ir_output_type | 输入 | 输出IR类型枚举值，支持取值请参见[IrOutputType](../IrOutputType.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrOutputDefV2 & | IR输出定义类自身引用，支持链式调用。 |

## 约束说明

无
