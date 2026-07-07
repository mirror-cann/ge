# GetDefaultValue

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取属性的默认值。

## 函数原型

```c++
const AttrValue &GetDefaultValue() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | const AttrValue & | 返回通过[DefaultValue](DefaultValue.md)设置的属性默认值的常量引用。 |

## 约束说明

无
