# SubgraphInput构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler.so

## 功能说明

SubgraphInput构造函数和析构函数。

## 函数原型

```c++
SubgraphInput()
explicit SubgraphInput(std::vector<NodeIo> node_inputs)
~SubgraphInput()
SubgraphInput(const SubgraphInput &other) noexcept
SubgraphInput &operator=(SubgraphInput &&other) noexcept
SubgraphInput &operator=(const SubgraphInput &other) noexcept
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| node_inputs | 输入 | 节点输入。 |

## 返回值说明

无

## 约束说明

无
