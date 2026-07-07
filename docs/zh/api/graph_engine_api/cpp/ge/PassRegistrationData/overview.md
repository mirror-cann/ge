# 简介

用于注册和管理自定义Pass（处理阶段）的类。

## 需要包含的头文件

```c++
#include <register/register_custom_pass.h>
```

## Public成员函数

```c++
PassRegistrationData() = default
~PassRegistrationData() = default
PassRegistrationData(std::string pass_name)
PassRegistrationData &CustomPassFn(const CustomPassFunc &custom_pass_fn)
std::string GetPassName() const
CustomPassFunc GetCustomPassFn() const
PassRegistrationData &Stage(const CustomPassStage stage)
CustomPassStage GetStage() const
PassRegistrationData &CustomAllocateStreamPassFn(const CustomAllocateStreamPassFunc &allocate_stream_pass_fn)
CustomAllocateStreamPassFunc GetCustomAllocateStreamPass() const
```
