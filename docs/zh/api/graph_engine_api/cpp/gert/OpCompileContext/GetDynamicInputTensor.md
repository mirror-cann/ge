# GetDynamicInputTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/op\_compile\_context.h\>
- 库文件：liblowering.so

## 功能说明

基于算子IR原型定义，获取DYNAMIC\_INPUT（动态输入）类型的输入Tensor指针，需要IR原型索引和相对索引两个参数来定位动态输入端口实例化后的具体输入项，用于处理可变数量输入的算子场景。

## 函数原型

```c++
const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ir_index | 输入 | IR原型定义中的索引，标识是哪个动态输入端口。 |
| relative_index | 输入 | 相对索引，标识该动态输入实例化后的具体第几个输入。 |

## 返回值说明

输入Tensor指针，异常时返回空指针。

## 约束说明

无
