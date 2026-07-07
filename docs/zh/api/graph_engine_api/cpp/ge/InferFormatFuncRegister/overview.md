# 简介

算子InferFormat函数注册接口，此接口被其他头文件引用，一般不用由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
InferFormatFuncRegister(const std::string &operator_type, const InferFormatFunc &infer_format_func)
InferFormatFuncRegister(const char_t *const operator_type, const InferFormatFunc &infer_format_func)
~InferFormatFuncRegister() = default
```
