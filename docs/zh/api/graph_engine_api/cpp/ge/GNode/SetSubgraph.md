# SetSubgraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

更新指定输出端口的tensor格式。

## 函数原型

```c++
graphStatus SetSubgraph(const AscendString &subgraph_ir_name, const Graph &subgraph)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| subgraph_ir_name | 输入 | 算子IR注册时的子图名称，以.GRAPH(X)的注册方式举例，subgraph_ir_name对应"X" |
| subgraph | 输入 | 要绑定节点上的子图实例 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
