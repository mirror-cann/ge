# RemoveSubgraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

根据name移除子图。

## 函数原型

```c++
graphStatus RemoveSubgraph(const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 子图名称。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS：子图移除成功<br>其他：子图移除失败 |

## 约束说明

无
