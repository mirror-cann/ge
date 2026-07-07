# 简介

逻辑流分配信息。

## 需要包含的头文件

```c++
#include <ge/ge_graph_compile_summary.h>
```

## Public成员函数

```c++
AscendString ToStringInfo() const
int64_t GetLogicalStreamId() const
AscendString GetUsrStreamLabel() const
bool IsAssignedByStreamPass() const
std::vector<int64_t> GetAttachedStreamIds() const
size_t GetPhysicalStreamNum() const
size_t GetHcclFollowedStreamNum() const
const std::vector<GNode> &GetAllNodes() const
```
