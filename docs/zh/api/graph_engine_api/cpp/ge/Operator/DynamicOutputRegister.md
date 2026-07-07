# DynamicOutputRegister

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

算子动态输出注册。

## 函数原型

```c++
void DynamicOutputRegister(const char_t *name, const uint32_t num, bool is_push_back = true)
void DynamicOutputRegister(const char_t *name, const uint32_t num, const char_t *datatype_symbol, bool is_push_back = true)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子的动态输出名。 |
| num | 输入 | 添加的动态输出个数。 |
| datatype_symbol | 输入 | 动态输出的数据类型。 |
| is_push_back | 输入 | - true表示在尾部追加动态输出。<br>  - false表示在头部追加动态输出。 |

## 返回值说明

无。

## 异常处理

无。

## 约束说明

无。
