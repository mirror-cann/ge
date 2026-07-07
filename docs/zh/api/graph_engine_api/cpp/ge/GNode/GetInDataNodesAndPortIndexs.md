# GetInDataNodesAndPortIndexs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

获取算子指定输入端口对应的对端算子以及对端算子的输出端口号。

## 函数原型

```c++
std::pair<GNodePtr, int32_t> GetInDataNodesAndPortIndexs(const int32_t index) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 算子的输入端口号。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | std::pair<GNodePtr, int32_t > | 对端输入算子以及对端输入算子的输出端口号，空表示没有对端算子。 |

## 约束说明

无
