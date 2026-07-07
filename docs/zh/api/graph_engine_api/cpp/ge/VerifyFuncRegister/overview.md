# 简介

算子verifyFunc函数注册接口，此接口被其他头文件引用，一般不用由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
VerifyFuncRegister(const std::string &operator_type, const VerifyFunc &verify_func)
VerifyFuncRegister(const char_t *const operator_type, const VerifyFunc &verify_func)
~VerifyFuncRegister() = default
```
