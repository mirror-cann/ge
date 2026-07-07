# InferShape

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_utils.h\>
- 库文件：libge\_compiler.so

## 功能说明

给定输入shape，对传入的graph做全图shape推导。

本接口只做shape推导，不对图做任何其他优化（如常量折叠、死边消除等）。

## 函数原型

```c++
static Status InferShape(const Graph &graph, const std::vector<Shape> &input_shape)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 待推导的图。 |
| input_shape | 输入 | 图输入对应的shape。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | SUCCESS：推导成功。<br>FAILED：推导失败。 |

## 约束说明

无
