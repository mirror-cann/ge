# 简介

用于定义图结构匹配的模板。该模板接收一个Graph，用Graph来描述待匹配的图结构。

## 需要包含的头文件

```c++
#include <ge/fusion/pattern.h>
```

## Public成员函数

```c++
explicit Pattern(Graph &&pattern_graph)
const Graph &GetGraph() const
Pattern &CaptureTensor(const NodeIo &node_output)
Status GetCapturedTensors(std::vector<NodeIo> &node_outputs) const
```
