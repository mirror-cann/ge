# REG\_DECOMPOSE\_PASS

## 产品支持情况

全量芯片支持。

## 头文件

\#include <ge/fusion/pass/decompose\_pass.h\>

## 功能说明

开发人员可以选择将1对N场景下的改图函数注册到框架中，由框架在编译最开始调用自定义改图Pass，调用REG\_DECOMPOSE\_PASS进行自定义Pass注册。

## 函数原型

```c++
REG_DECOMPOSE_PASS(pass_class, decompose_op_types)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| pass_class | 输入 | 自定义Pass的名称。 |
| decompose_op_types | 输入 | 需要替换的算子类型。 |

## 返回值说明

无

## 约束说明

无
