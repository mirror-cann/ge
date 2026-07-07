# REGISTER\_CUSTOM\_PASS

## 产品支持情况

全量芯片支持。

## 头文件

\#include <register/register\_custom\_pass.h\>

## 功能说明

开发人员可以选择将改图函数注册到框架中，由框架在编译最开始调用自定义改图Pass，调用REGISTER\_CUSTOM\_PASS进行自定义Pass注册。

## 函数原型

```c++
REGISTER_CUSTOM_PASS(name)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 自定义Pass的名称。 |

## 返回值说明

无

## 约束说明

调用时以REGISTER\_CUSTOM\_PASS开始，以“.”连接CustomPassFn等接口。例如：

```c++
#include "register/register_custom_pass.h"
REGISTER_CUSTOM_PASS("pass_name").CustomPassFn(CustomPassFunc);
```
