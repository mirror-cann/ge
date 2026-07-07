# 简介

基于Pattern匹配的融合Pass基类。子类可通过实现虚方法来定义pattern和replacement。由父类进行匹配和替换。

## 需要包含的头文件

```c++
#include <ge/fusion/pass/pattern_fusion_pass.h>
```

## Public成员函数

```c++
PatternFusionPass()
explicit PatternFusionPass(std::unique_ptr<PatternMatcherConfig> match_config)
Status Run(GraphPtr &graph, CustomPassContext &pass_context) override
```
