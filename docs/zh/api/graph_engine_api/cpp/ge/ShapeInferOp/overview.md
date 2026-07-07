# 简介

自定义算子的Shape推理接口，适用于算子基于推理上下文执行形状和数据类型推导的场景。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus InferShape(gert::InferShapeContext *ctx) = 0
virtual graphStatus InferDataType(gert::InferDataTypeContext *ctx) = 0
```
