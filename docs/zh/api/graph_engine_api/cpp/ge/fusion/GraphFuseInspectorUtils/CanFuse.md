# CanFuse

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/graph\_fuse\_inspector\_utils.h\>
- 库文件：libgraph\_base.so

## 功能说明

判断待融合节点集合是否可融合。判断条件：

1. 节点上的stream lable表达图上的分流策略，当传入节点stream label不一致时，无法确定用户意图，即无法确定新节点该继承谁的标签，因此传入节点列表的stream label不一致时，判断为无法融合。

2. 如果传入节点列表融合成单个节点出现环，判断为无法融合，注意传入节点替换为多个节点等其他场景不在此处的成环检测范围内。

## 函数原型

```c++
static bool CanFuse(const std::vector<GNode> &nodes_before_fuse, AscendString &failed_reason)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| nodes_before_fuse | 输入 | 融合前节点列表（列表内所有节点需连通）。 |
| failed_reason | 输出 | 不支持融合的原因 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | bool | - true：可融合。<br>  - false：不可融合（failed_reason填充具体原因）。 |

## 约束说明

无
