# SetOutput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置图的输出节点。

## 函数原型

```c++
static int64_t SetOutput(const EsTensorHolder &tensor, int64_t index);
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | 输出张量持有者。 |
| index | 输入 | 输出索引，从0开始计数。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | int64_t | 返回操作结果，成功为0，失败返回非0值。 |

## 约束说明

无

## 调用示例

```c++
// 1. 创建图构建器（EsGraphBuilder）
EsGraphBuilder builder("graph_name");
// 2. 添加 2 个输入节点
EsTensorHolder [data0, data1] = builder.CreateInputs<2>();
// 3. 添加中间节点，C++中，加减乘除等常用运算符被重载，可以直接使用
EsTensorHolder add = data0 + data1;
// 4. 设置图输出，add为第0个输出tensor
builder.SetOutput(add, 0);
```
