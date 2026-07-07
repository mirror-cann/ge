# EsCGraphBuilder构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

EsCGraphBuilder构造函数和析构函数。

## 函数原型

```c++
EsCGraphBuilder()
explicit EsCGraphBuilder(const char *name)
~EsCGraphBuilder()
EsCGraphBuilder(EsCGraphBuilder &&) noexcept
EsCGraphBuilder &operator=(EsCGraphBuilder &&) noexcept
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 图名称，按照指定的名称构造图构建器。 |

## 返回值说明

无

## 约束说明

无
