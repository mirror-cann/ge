# AttrDataType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置属性的数据类型。

## 函数原型

```c++
IrAttrDefV2 &AttrDataType(const char_t *attr_data_type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| attr_data_type | 输入 | 属性值的数据类型字符串。典型取值有：<br><br>  - "int"：整数类型<br>  - "float"：浮点类型<br>  - "string"：字符串类型<br>  - "bool"：布尔类型<br>  - "list_int"：整数列表<br>  - "list_float"：浮点列表 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrAttrDefV2 & | IR属性定义类自身引用，支持链式调用。 |

## 约束说明

无
