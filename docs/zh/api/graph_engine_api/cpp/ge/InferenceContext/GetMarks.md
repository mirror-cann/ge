# GetMarks

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

在资源类算子推理的上下文中，获取成对资源算子的标记。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
const std::vector<std::string> &GetMarks() const
void GetMarks(std::vector<AscendString> &marks) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| marks | 输出 | 资源类算子的标记。 |

## 返回值说明

资源类算子的标记。

## 异常处理

无。

## 约束说明

无。
