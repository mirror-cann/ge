# GetInControlNodes

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

获取算子的控制输入节点。

## 函数原型

```c++
std::vector<GNodePtr> GetInControlNodes() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | vector\<GNodePtr> | 算子的控制输入节点列表，空表示没有控制算子，算子的控制输入可能有多个。 |

## 约束说明

无
