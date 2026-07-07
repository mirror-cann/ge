# 简介

GE改图相关的工具类。如shape推导，校验节点是否支持，部分图优化工具方法等。

## 需要包含的头文件

```c++
#include <ge/ge_utils.h>
```

## Public成员函数

```c++
static Status InferShape(const Graph &graph, const std::vector<Shape> &input_shape)
static Status CheckNodeSupportOnAicore(const GNode &node, bool &is_supported, AscendString &unsupported_reason)
```
