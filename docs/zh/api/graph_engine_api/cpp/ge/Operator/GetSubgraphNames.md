# GetSubgraphNames

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

获取算子IR信息中子图名称列表。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
std::vector<std::string> GetSubgraphNames() const
graphStatus GetSubgraphNames(std::vector<AscendString> &names) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| names | 输出 | 获取一个算子的子图名称列表。 |

## 返回值说明

graphStatus类型：GRAPH\_SUCCESS，代表成功；GRAPH\_FAILED，代表失败。

## 异常处理

无。

## 约束说明

无。
