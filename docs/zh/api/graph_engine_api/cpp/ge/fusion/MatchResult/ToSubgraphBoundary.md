# ToSubgraphBoundary

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取匹配结果对应的子图边界。

## 函数原型

```c++
std::unique_ptr<SubgraphBoundary> ToSubgraphBoundary() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::unique_ptr\<SubgraphBoundary> | 一个子图边界定义，详见[SubgraphBoundary](../SubgraphBoundary/SubgraphBoundary.md)。 |

## 约束说明

无
