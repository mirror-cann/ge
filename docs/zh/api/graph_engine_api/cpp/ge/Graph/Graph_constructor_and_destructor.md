# Graph构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/graph.h\>
- 库文件：libgraph.so

## 功能说明

Graph构造函数和析构函数。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string且传name参数的接口。默认无参的构造函数构造的是一个非法的Graph对象。

```c++
explicit Graph(const std::string& name)
explicit Graph(const char *name)
Graph()
~Graph()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | Graph名称，按照指定的名称构造Graph。 |

## 返回值说明

Graph构造函数返回Graph类型的对象。

## 约束说明

无
