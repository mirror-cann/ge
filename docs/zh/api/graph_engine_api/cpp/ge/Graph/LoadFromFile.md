# LoadFromFile

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

从文件中读取Graph。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
graphStatus LoadFromFile(const std::string &file_name)
graphStatus LoadFromFile(const char_t *file_name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| file_name | 输入 | 文件路径和文件名。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

仅支持读取air格式的文件。
