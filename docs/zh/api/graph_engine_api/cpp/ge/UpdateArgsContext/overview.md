# 简介

静态图场景自定义算子args刷新的上下文类，继承自EagerOpExecutionContext ，提供args刷新所需的KernelArgs获取，用于自定义算子更新args。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
const KernelArgs* GetKernelArgs(Placement placement, size_t index) const
```
