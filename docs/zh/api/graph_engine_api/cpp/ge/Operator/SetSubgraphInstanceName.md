# SetSubgraphInstanceName

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

设置子图实例的名称。

## 函数原型

```c++
graphStatus SetSubgraphInstanceName(const uint32_t index, const char_t *name);
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 子图索引。 |
| name | 输入 | 子图实例的名称。 |

## 返回值说明

graphStatus类型：成功，返回GRAPH\_SUCCESS，否则，返回GRAPH\_FAILED。

## 异常处理

无

## 约束说明

无
