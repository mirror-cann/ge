# Replace

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/graph\_rewriter.h\>
- 库文件：libge\_compiler.so

## 功能说明

给定SubgraphBoundary，将边界内算子替换为replacement。

## 函数原型

```c++
static Status Replace(const SubgraphBoundary &subgraph, const Graph &replacement)
static Status Replace(const SubgraphBoundary &subgraph, Graph &&replacement)
static Status Replace(const SubgraphBoundary &subgraph, Graph &&replacement, CustomPassContext &ctx)
static Status Replace(const SubgraphBoundary &subgraph, const Graph &replacement, CustomPassContext &ctx)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| subgraph | 输入 | 要替换的子图边界。 |
| replacement | 输入 | 替换图。（用于描述子图即将替换为什么图结构） |
| ctx | 输入 | Pass上下文。使用该重载时会自动执行可融合检查（[CanFuse](../GraphFuseInspectorUtils/CanFuse.md)）与融合结果上报（[ReportFuse](../GraphFuseInspectorUtils/ReportFuse.md），且通过ctx中的pass_name记录融合来源。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | 是否替换成功。 |

## 约束说明

无
