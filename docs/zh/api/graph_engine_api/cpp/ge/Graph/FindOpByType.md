# FindOpByType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

基于算子类型，获取缓存在Graph中的所有指定类型的op对象。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus FindOpByType(const std::string &type, std::vector<ge::Operator> &ops) const
graphStatus FindOpByType(const char_t *type, std::vector<ge::Operator> &ops) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| type | 输入 | 需要获取的算子类型。 |
| ops | 输出 | 返回用户所需要的op对象。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
