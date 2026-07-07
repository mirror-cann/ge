# GetDynamicInputNum

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

获取算子的动态Input的实际个数。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
int32_t GetDynamicInputNum(const std::string &name) const
int32_t GetDynamicInputNum(const char_t *name) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子的动态Input名。 |

## 返回值说明

实际动态Input的个数。

当name非法，或者算子无动态Input时，返回-1。

## 约束说明

无。
