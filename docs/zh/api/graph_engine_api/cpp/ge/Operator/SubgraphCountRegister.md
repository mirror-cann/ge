# SubgraphCountRegister

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

子图注册。

## 函数原型

```c++
void SubgraphCountRegister(const char_t *ir_name, uint32_t count)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| ir_name | 输入 | 子图名称。 |
| count | 输入 | 动态个数子图场景（子图数量不固定），注册count个数的子图，子图名称是ir_name_0 ~ ir_name_n， n < count。 |

## 返回值说明

无。

## 异常处理

无。

## 约束说明

无。
