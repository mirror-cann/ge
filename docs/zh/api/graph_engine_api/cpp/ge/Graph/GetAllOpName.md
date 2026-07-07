# GetAllOpName

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

获取Graph中已注册的所有缓存算子的名称列表。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++

graphStatus GetAllOpName(std::vector<std::string> &op_name) const
graphStatus GetAllOpName(std::vector<AscendString> &names) const

```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| op_name | 输出 | 返回Graph中的所有算子的名称。 |
| names | 输出 | 返回Graph中的所有算子的名称。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | SUCCESS：操作成功<br>FAILED：操作失败 |

## 约束说明

此接口为非必需接口，与[AddOp](AddOp.md)搭配使用。
