# REG\_AUTO\_MAPPING\_OP

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/custom.h\>

## 功能说明

开发人员可以选择将自定义算子注册到框架中，由框架在编译最开始调用REG\_AUTO\_MAPPING\_OP进行自定义算子注册。

## 函数原型

```c++
REG_AUTO_MAPPING_OP(custom_op_class)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| custom_op_class | 输入 | 自定义算子名称。 |

## 返回值说明

无

## 约束说明

无
