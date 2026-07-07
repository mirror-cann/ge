# 简介

算子InferValueRangeFuncRegister函数注册接口，此接口被其他头文件引用，一般不由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
InferValueRangeFuncRegister(const char_t *const operator_type, const WHEN_CALL when_call,
const InferValueRangeFunc &infer_value_range_func)
InferValueRangeFuncRegister(const char_t *const operator_type)
~InferValueRangeFuncRegister() = default
```
