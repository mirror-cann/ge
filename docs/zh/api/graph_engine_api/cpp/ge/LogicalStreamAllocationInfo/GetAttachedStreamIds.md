# GetAttachedStreamIds

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取当前逻辑流的附属逻辑从流ID。

## 函数原型

```c++
std::vector<int64_t> GetAttachedStreamIds() const
```

## 参数说明

无

## 返回值说明

int64\_t类型的向量，包含附属逻辑从流ID。

## 约束说明

无
