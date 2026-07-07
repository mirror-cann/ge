# 简介

内部关联接口，此接口被其他头文件引用，一般不用由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
static Operator CreateOperator(const std::string &operator_name, const std::string &operator_type)
static Operator CreateOperator(const char_t *const operator_name, const char_t *const operator_type)
static graphStatus GetOpsTypeList(std::vector<std::string> &all_ops)
static graphStatus GetOpsTypeList(std::vector<AscendString> &all_ops)
static bool IsExistOp(const std::string &operator_type)
static bool IsExistOp(const char_t *const operator_type)
```
