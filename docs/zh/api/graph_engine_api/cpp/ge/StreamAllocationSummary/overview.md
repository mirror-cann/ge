# 简介

流分配的概要信息。

## 需要包含的头文件

```c++
#include <ge/ge_graph_compile_summary.h>
```

## Public成员函数

```c++
const std::map<AscendString, AscendString> &ToStreamGraph() const;
const std::map<AscendString, std::vector<LogicalStreamAllocationInfo>> &GetAllLogicalStreamInfos() const;
```
