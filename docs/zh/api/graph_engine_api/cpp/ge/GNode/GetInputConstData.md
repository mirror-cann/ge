# GetInputConstData

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

获取输入为Const节点的值。

如果算子的输入是Const节点，会返回Const节点的值，否则返回失败。支持跨子图获取输入是否是Const及Const下的value。

## 函数原型

```c++
graphStatus GetInputConstData(const int32_t index, Tensor &data) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 算子的输入端口号。 |
| data | 输出 | 输入Const的值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS表示成功，GRAPH_NODE_WITHOUT_CONST_INPUT输入非Const，其他表示失败。 |

## 约束说明

无
