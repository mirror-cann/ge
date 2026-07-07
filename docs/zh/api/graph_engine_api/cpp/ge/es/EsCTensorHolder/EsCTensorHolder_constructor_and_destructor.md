# EsCTensorHolder构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

EsCTensorHolder构造函数和析构函数。

## 函数原型

```c++
EsCTensorHolder(EsCGraphBuilder &owner, const ge::GNode &producer, int32_t index)
~EsCTensorHolder()
EsCTensorHolder(EsCTensorHolder &&) noexcept
EsCTensorHolder &operator=(EsCTensorHolder &&) noexcept
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| owner | 输入 | EsCGraphBuilder引用，图构建器。 |
| producer | 输入 | GNode引用，生产者节点。 |
| index | 输入 | 输出索引。 |

## 返回值说明

EsCTensorHolder构造函数返回EsCTensorHolder类型的对象。

## 约束说明

无
