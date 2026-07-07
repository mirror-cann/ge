# GetSubgraphBuilder

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

根据子图名称获取算子对应的子图构建的函数对象。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
SubgraphBuilder GetSubgraphBuilder(const std::string &name) const
SubgraphBuilder GetSubgraphBuilder(const char_t *name) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 子图名称。 |

## 返回值说明

SubgraphBuilder对象。

## 异常处理

无。

## 约束说明

无。
