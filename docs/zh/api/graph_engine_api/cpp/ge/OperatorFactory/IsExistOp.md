# IsExistOp

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/operator\_factory.h\>
- 库文件：libgraph.so

## 功能说明

查询指定的算子类型是否支持。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
static bool IsExistOp(const std::string &operator_type)
static bool IsExistOp(const char_t *const operator_type)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| operator_type | 输入 | 算子类型。 |

## 返回值说明

- true：存在此算子。
- false：不存在此算子。

## 约束说明

无。
