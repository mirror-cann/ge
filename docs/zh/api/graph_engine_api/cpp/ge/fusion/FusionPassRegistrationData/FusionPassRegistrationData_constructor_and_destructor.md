# FusionPassRegistrationData构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/fusion\_pass\_reg.h\>
- 库文件：libge\_compiler.so

## 功能说明

FusionPassRegistrationData构造函数和析构函数。

## 函数原型

```c++
FusionPassRegistrationData() = default
~FusionPassRegistrationData() = default
explicit FusionPassRegistrationData(const AscendString &pass_name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pass_name | 输入 | 融合Pass名称。 |

## 返回值说明

无

## 约束说明

无
