# 简介

用于执行算子的上下文环境。

为算子在设备上运行时提供输入输出管理、内存分配、流控制等运行时支持。

## 需要包含的头文件

```c++
#include <exe_graph/runtime/eager_op_execution_context.h>
```

## Public成员函数

```c++
rtStream GetStream() const
const Tensor *GetInputTensor(size_t index) const
const Tensor *GetRequiredInputTensor(size_t ir_index) const
const Tensor *GetOptionalInputTensor(size_t ir_index) const
const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const
Tensor *MallocOutputTensor(size_t index, const StorageShape &shape, const StorageFormat &format, ge::DataType dtype)
Tensor *MakeOutputRefInput(size_t output_index, size_t input_index) const
const KernelArgs* MallocReadOnlyDevArgs(void *host_args, size_t args_size) const
void *MallocWorkSpace(size_t size)
const Tensor *GetOutputTensor(size_t index) const
```
