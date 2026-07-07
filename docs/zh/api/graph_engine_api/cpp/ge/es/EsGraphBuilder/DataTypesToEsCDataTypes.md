# DataTypesToEsCDataTypes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

ge::DataType集合转换为C\_DataType集合。

## 函数原型

```c++
inline std::vector<C_DataType> DataTypesToEsCDataTypes(const std::vector<ge::DataType> &data_types)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| data_types | 输入 | ge::DataType类型容器。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector<C_DataType> | C_DataType类型容器。 |

## 约束说明

无
