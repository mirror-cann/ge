# GetOpsTypeList

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/operator\_factory.h\>
- 库文件：libgraph.so

## 功能说明

获取系统支持的所有算子类型列表。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
static graphStatus GetOpsTypeList(std::vector<std::string> &all_ops)
static graphStatus GetOpsTypeList(std::vector<AscendString> &all_ops)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| all_ops | 输出 | 算子类型列表。 |

## 返回值说明

graphStatus类型：GRAPH\_SUCCESS，代表成功；GRAPH\_FAILED，代表失败。

## 约束说明

无。
