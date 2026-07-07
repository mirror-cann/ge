# InputType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输入定义的类型，决定该输入是必需、可选还是动态。

## 函数原型

```c++
IrInputDefV2 &InputType(IrInputType ir_input_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ir_input_type | 输入 | 输入定义的枚举，支持取值请参见[IrInputType](../IrInputType.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrInputDefV2 & | IR输入定义类自身引用，支持链式调用。 |

## 约束说明

无
