# 简介

算子infershape函数注册接口，此接口被其他头文件引用，一般不用由算子开发者直接调用。

## 需要包含的头文件

```c++
#include <graph/operator_factory.h>
```

## Public成员函数

```c++
InferShapeFuncRegister (const std::string &operator_type, const InferShapeFunc &infer_shape_func)
InferShapeFuncRegister(const char *const operator_type, const InferShapeFunc &infer_shape_func)
~ InferShapeFuncRegister()
```
