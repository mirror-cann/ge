# 简介

融合Pass基类，描述了Pass执行的入参信息。

## 需要包含的头文件

```c++
#include <ge/fusion/pass/fusion_base_pass.h>
```

## Public成员函数

```c++
virtual Status Run(GraphPtr &graph, CustomPassContext &pass_context) = 0
```
