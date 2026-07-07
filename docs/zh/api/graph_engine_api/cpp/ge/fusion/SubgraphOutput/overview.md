# 简介

用于描述Subgraph的一个输出Tensor。

## 需要包含的头文件

```c++
#include <ge/fusion/subgraph_boundary.h>
```

## Public成员函数

```c++
SubgraphOutput()
explicit SubgraphOutput(const NodeIo &node_output)
SubgraphOutput(const SubgraphOutput &other) noexcept
SubgraphOutput &operator=(SubgraphOutput &&other) noexcept
SubgraphOutput &operator=(const SubgraphOutput &other) noexcept
Status SetOutput(const NodeIo &node_output)
Status GetOutput(NodeIo &node_output) const
```
