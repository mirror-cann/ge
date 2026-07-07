# InferValueRangeFuncRegister构造函数和析构函数

## 头文件

\#include <graph/operator\_factory.h\>

## 功能说明

InferValueRangeFuncRegister构造函数和析构函数。

## 函数原型

```c++
InferValueRangeFuncRegister(const char_t *const operator_type, const WHEN_CALL when_call,
const InferValueRangeFunc &infer_value_range_func)
InferValueRangeFuncRegister(const char_t *const operator_type)
~InferValueRangeFuncRegister() = default
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| operator_type | 输入 | 算子类型。 |
| when_call | 输入 | infer函数的调用场景。 |
| infer_value_range_func | 输入 | 算子InferValueRange函数。 |

## 返回值说明

InferValueRangeFuncRegister构造函数返回InferValueRangeFuncRegister类型的对象。

## 约束说明

算子InferValueRangeFuncRegister函数注册接口，此接口被其他头文件引用，一般不由算子开发者直接调用。
