# GetSubGraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

根据name获取子图。

## 函数原型

```c++
GraphPtr GetSubGraph(const char *name) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 子图名称。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | GraphPtr | 返回子图指针。 |

## 约束说明

无
