# SubgraphBoundary构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/subgraph\_boundary.h\>
- 库文件：libge\_compiler

## 功能说明

SubgraphBoundary构造函数和析构函数。

## 函数原型

```c++
SubgraphBoundary()
SubgraphBoundary(std::vector<SubgraphInput> inputs, std::vector<SubgraphOutput> outputs)
~SubgraphBoundary()
SubgraphBoundary(const SubgraphBoundary &other) noexcept
SubgraphBoundary &operator=(SubgraphBoundary &&other) noexcept
SubgraphBoundary &operator=(const SubgraphBoundary &other) noexcept
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| inputs | 输入 | 子图边界所有输入。 |
| outputs | 输出 | 子图边界所有输出。 |

## 返回值说明

无

## 约束说明

无
