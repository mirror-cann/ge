# EsCreateGraphBuilder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建一个图构建器实例。

## 函数原型

```c
EsCGraphBuilder *EsCreateGraphBuilder(const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 可选，图的名字，如果为空则默认为"graph"。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCGraphBuilder * | 返回图构建器指针，失败返回nullptr。 |

## 约束说明

无

## 调用示例

```c
EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
```
