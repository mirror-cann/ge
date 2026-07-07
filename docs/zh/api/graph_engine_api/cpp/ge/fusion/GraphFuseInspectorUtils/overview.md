# 简介

图融合检查工具类，为自定义融合Pass提供融合可行性判断和结果上报能力。

## 需要包含的头文件

```c++
#include <ge/fusion/graph_fuse_inspector_utils.h>
```

## Public成员函数

```c++
static bool CanFuse(const std::vector<GNode> &nodes_before_fuse, AscendString &failed_reason)
static Status ReportFuse(const std::vector<GNode> &nodes_before_fuse, const std::vector<GNode> &nodes_after_fuse, CustomPassContext &ctx)
```
