# aclgrphDumpGraph

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

将输入的Graph导出到文本中。

## 函数原型

```c++
graphStatus aclgrphDumpGraph(const ge::Graph &graph, const char_t *file, const size_t len)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 输入的Graph。 |
| file | 输入 | 输出文件名。 |
| len | 输入 | 输出文件名的字符长度。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
