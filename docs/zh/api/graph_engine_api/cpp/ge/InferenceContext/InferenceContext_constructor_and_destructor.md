# InferenceContext构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/inference\_context.h\>
- 库文件：libgraph.so

## 功能说明

InferenceContext对象的构造函数和析构函数。

## 函数原型

```c++
~InferenceContext() = default
InferenceContext(const InferenceContext &context) = delete
InferenceContext(const InferenceContext &&context) = delete
InferenceContext &operator=(const InferenceContext &context) = delete
InferenceContext &operator=(const InferenceContext &&context) = delete
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| context | 输入 | InferenceContext内容，供初始化使用。 |

## 返回值说明

InferenceContext构造函数返回InferenceContext类型的对象。

## 异常处理

无。

## 约束说明

无。
