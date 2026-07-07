# GetCTensorHolder

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取底层的C语言张量持有者指针，返回值的生命周期受EsGraphBuilder管理，使用方不能释放。

## 函数原型

```c++
EsCTensorHolder *GetCTensorHolder() const
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 返回C语言张量持有者指针。 |

## 约束说明

无
