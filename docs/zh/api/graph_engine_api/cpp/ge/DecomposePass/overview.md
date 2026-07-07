# 简介

用于实现一对多场景融合Pass的基类。该类提供根据节点类型在图中查找节点，根据子类提供的Replacement结构改图的功能。

## 需要包含的头文件

```c++
#include <ge/fusion/pass/decompose_pass.h>
```

## Public成员函数

```c++
explicit DecomposePass(const std::vector<AscendString> &op_types)
Status Run(GraphPtr &graph, CustomPassContext &pass_context) override
```
