# MallocReadOnlyDevArgs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <exe\_graph/runtime/eager\_op\_execution\_context.h\>
- 库文件：liblowering.so

## 功能说明

为host侧args buffer分配设备侧内存，并返回只读KernelArgs指针。

本接口在模型执行前将host\_args内容同步到设备侧，接口返回后调用方可立即释放host\_args，无需手动H2D拷贝。

## 函数原型

```c++
 const KernelArgs* MallocReadOnlyDevArgs(void *host_args, size_t args_size) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| host_args | 输入 | Host侧args buffer指针，接口返回后调用方可立即释放。 |
| args_size | 输入 | Args buffer大小（字节）。 |

## 返回值说明

只读KernelArgs指针，失败返回nullptr。

## 约束说明

无
