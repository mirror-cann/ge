# GetInputTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

根据输入index，获取输入tensor指针。

## 函数原型

```c++
const Tensor *GetInputTensor(size_t index) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 输入index。 |

## 返回值说明

Tensor指针，异常时返回空指针。

## 约束说明

无
