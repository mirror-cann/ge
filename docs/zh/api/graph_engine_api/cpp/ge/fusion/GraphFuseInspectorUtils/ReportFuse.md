# ReportFuse

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/graph\_fuse\_inspector\_utils.h\>
- 库文件：libgraph\_base.so

## 功能说明

上报融合结果，在完成对图的修改后，需要上报融合结果以完成图连接矩阵的更新、维测信息记录等操作。

接口内部逻辑简要如下：

1. 新节点的opdesc记录pass name。
2. 更新在CanFuse中用于检测成环的连接矩阵。
3. 记录匹配次数与生效次数，对应信息落盘至fusion\_result.json。

## 函数原型

```c++
static Status ReportFuse(const std::vector<GNode> &nodes_before_fuse, const std::vector<GNode> &nodes_after_fuse, CustomPassContext &ctx)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| nodes_before_fuse | 输入 | 融合前节点列表（列表内所有节点需连通）。 |
| nodes_after_fuse | 输入 | 融合后新节点列表（列表内所有节点需连通）。nodes_after_fuse为空表示删除但不新增节点的场景。 |
| ctx | 输入 | Pass上下文，使用ctx.GetPassName()记录pass name。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：上报成功<br>FAILED：上报失败 |

## 约束说明

该接口必须在改图后且释放删除节点前调用。
