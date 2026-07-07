# 简介

自定义算子的基类。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus Execute(gert::EagerOpExecutionContext *ctx) = 0;
virtual ~BaseCustomOp() = default;
```
