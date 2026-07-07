# EsCreateVariable

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

创建变量节点。

## 函数原型

```c
EsCTensorHolder *EsCreateVariable(EsCGraphBuilder *graph, int32_t index, const char *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 图构建器指针。 |
| index | 输入 | 节点索引。 |
| name | 输入 | 节点名称。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 成功返回创建的Tensor持有者指针，失败返回nullptr。 |

## 约束说明

无

## 调用示例

```c
EsCGraphBuilder *graph = EsCreateGraphBuilder("test_graph");
auto variable = EsCreateVariable(graph, 0, "var0");
```
