# MakeOutputRefInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

指定某输出的内存地址引用自某个输入。

## 函数原型

```c++
Tensor *MakeOutputRefInput(size_t output_index, size_t input_index) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| output_index | 输入 | 输出索引。 |
| input_index | 输入 | 输入索引。 |

## 返回值说明

output\_index对应的输出Tensor指针。

## 约束说明

无
