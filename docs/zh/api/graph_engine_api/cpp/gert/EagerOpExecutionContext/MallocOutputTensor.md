# MallocOutputTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

为某个输出Tensor申请Device内存，同时初始化输出Tensor的基本信息。

该输出Tensor的内存由Context构造方管理。接口调用者不需要主动释放。

## 函数原型

```c++
Tensor *MallocOutputTensor(size_t index, const StorageShape &shape, const StorageFormat &format, ge::DataType dtype)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| index | 输入 | 输出索引。 |
| shape | 输入 | 输出tensor的shape。 |
| format | 输入 | 输出tensor的format。 |
| dtype | 输入 | 输出tensor的data type。 |

## 返回值说明

Tensor指针，异常时返回空指针。

## 约束说明

无
