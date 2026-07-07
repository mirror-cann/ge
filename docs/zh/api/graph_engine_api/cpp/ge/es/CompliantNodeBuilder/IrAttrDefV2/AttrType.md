# AttrType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置属性类型（必需/可选）。

## 函数原型

```c++
IrAttrDefV2 &AttrType(IrAttrType ir_attr_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ir_attr_type | 输入 | 属性IR类型枚举值。具体取值请参见[IrAttrType](../IrInputType.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrAttrDefV2 & | IR属性定义类自身引用，支持链式调用。 |

## 约束说明

无
