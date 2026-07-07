# 简介

自定义算子的地址刷新能力接口。静态图场景，继承此接口的算子会在I/O地址变化时被框架回调UpdateHostArgs方法，用于刷新kernel args中的地址引用。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) = 0
```
