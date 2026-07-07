# GetDynamicInputTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

基于算子IR原型定义，获取DYNAMIC\_INPUT类型的输入Tensor指针。

## 函数原型

```c++
const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| ir_index | 输入 | IR原型定义中的index。 |
| relative_index | 输入 | 该输入实例化后的相对index，例如某个DYNAMIC_INPUT实例化了3个输入，那么relative_index的有效范围是[0,2]。 |

## 返回值说明

Tensor指针，异常时返回空指针。

## 约束说明

无
