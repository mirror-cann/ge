# KernelLaunchInfo构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

KernelLaunchInfo构造函数和析构函数。

## 函数原型

```c++
~KernelLaunchInfo()
KernelLaunchInfo(const KernelLaunchInfo &other)
KernelLaunchInfo(KernelLaunchInfo &&other) noexcept
KernelLaunchInfo &operator=(const KernelLaunchInfo &other)
KernelLaunchInfo &operator=(KernelLaunchInfo &&other) noexcept
```

## 参数说明

无

## 返回值说明

无

## 约束说明

无
