# GeGraphsToEsCGraphs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

将动态子图转换为匿名指针类型子图容器。

## 函数原型

```c++
inline std::vector<EsCGraph *> GeGraphsToEsCGraphs(std::vector<std::unique_ptr<ge::Graph>> graphs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graphs | 输入 | 控制算子的动态子图入参容器。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::vector<EsCGraph *> | 匿名指针类型的子图容器。 |

## 约束说明

无
