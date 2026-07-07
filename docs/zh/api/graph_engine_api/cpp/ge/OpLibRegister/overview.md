# 简介

OpLibRegister类所提供的接口为内部关联接口，当前主要为自定义算子动态库提供注册功能，供自定义算子工程框架注册指定初始化函数，开发者无需关注。

## 需要包含的头文件

```c++
#include <register/op_lib_register.h>
```

## Public成员函数

```c++
explicit OpLibRegister(const char_t *vendor_name)
OpLibRegister(OpLibRegister &&other) noexcept
OpLibRegister(const OpLibRegister &other)
OpLibRegister &operator=(const OpLibRegister &) = delete
OpLibRegister &operator=(OpLibRegister &&) = delete
~OpLibRegister()
OpLibRegister &RegOpLibInit(OpLibInitFunc func)
```
