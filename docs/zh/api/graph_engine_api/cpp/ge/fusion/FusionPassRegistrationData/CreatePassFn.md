# CreatePassFn

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/fusion\_pass\_reg.h\>
- 库文件：libge\_compiler.so

## 功能说明

创建融合Pass注册函数。

## 函数原型

```c++
FusionPassRegistrationData &CreatePassFn(const CreateFusionPassFn &create_fusion_pass_fn)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| create_fusion_pass_fn | 输入 | 创建自定义Pass的执行函数。 |

## 返回值说明

FusionPassRegistrationData的引用，用于级联调用其成员函数。

## 约束说明

无

## 回调函数CreateFusionPassFn

用户自定义实现融合pass子类，调用注册宏：

```c++
REG_DECOMPOSE_PASS/
REG_FUSION_PASS
```

注册宏会自动生成构造融合pass子类对象的回调函数，即CreateFusionPassFn。该回调函数被用于在执行Pass之前，将Pass对象构造出来：

```c++
using CreateFusionPassFn = FusionBasePass *(*)();
```
