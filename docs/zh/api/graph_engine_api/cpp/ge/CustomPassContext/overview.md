# 简介

获取自定义Pass上下文对象。

## 需要包含的头文件

```c++
#include <register/register_custom_pass.h>
```

## Public成员函数

```c++
CustomPassContext()
virtual ~CustomPassContext() = default
void SetErrorMessage(const AscendString &error_message)
graphStatus GetOptionValue(const AscendString &option_key, AscendString &option_value) const
void SetPassName(const AscendString &pass_name)
AscendString GetPassName() const
```
