# 简介

算子注册接口，注册一个算子原型，此接口被其他头文件引用，一般不由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
OperatorCreatorRegister(const std::string &operator_type, OpCreator const &op_creator)
OperatorCreatorRegister(const char_t *const operator_type, OpCreatorV2 const &op_creator)
~OperatorCreatorRegister() = default
```
