# SubgraphOutput构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler.so

## 功能说明

SubgraphOutput构造函数和析构函数。

## 函数原型

```c++
SubgraphOutput()
explicit SubgraphOutput(const NodeIo &node_output)
~SubgraphOutput()
SubgraphOutput(const SubgraphOutput &other) noexcept
SubgraphOutput &operator=(SubgraphOutput &&other) noexcept
SubgraphOutput &operator=(const SubgraphOutput &other) noexcept
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_output | 输入 | 节点输出。 |

## 返回值说明

无

## 约束说明

无
