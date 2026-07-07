# DynamicInputRegisterByIndex

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

指定位置进行算子动态输入注册。

## 函数原型

```c++
void DynamicInputRegisterByIndex(const char_t *name, const uint32_t num, size_t index)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 算子的动态Input名。 |
| num | 输入 | 添加的动态Input个数。 |
| index | 输入 | 从index位置添加动态输入。 |

## 返回值说明

无。

## 异常处理

无。

## 约束说明

无。
