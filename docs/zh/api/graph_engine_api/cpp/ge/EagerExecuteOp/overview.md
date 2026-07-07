# 简介

自定义算子的基类，用于在“eager execution”（即时执行）模式下实现自定义的操作，即时执行模式通常用于深度学习框架中，允许在不构建完整的计算图的情况下直接执行操作，这可以提供更直观和灵活的调试体验。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus Execute(gert::EagerOpExecutionContext *ctx) = 0
```
