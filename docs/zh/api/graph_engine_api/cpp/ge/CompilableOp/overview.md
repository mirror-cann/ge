# 简介

自定义算子的构图编译接口，当算子进入GE构图编译流程后，若实现该接口，GE会回调Compile接口，完成算子编译相关处理。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus Compile(gert::OpCompileContext *ctx) = 0
```
