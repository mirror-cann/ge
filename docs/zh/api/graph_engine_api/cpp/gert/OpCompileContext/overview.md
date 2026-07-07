# 简介

算子编译阶段的上下文类，继承自ExtendedKernelContext，提供编译期所需的输入tensor获取（支持REQUIRED/OPTIONAL/DYNAMIC类型）、编译配置选项查询、平台信息获取等能力，用于自定义算子Compile接口的编译上下文传递。

## 需要包含的头文件

```c++
#include <exe_graph/runtime/op_compile_context.h
```

## Public成员函数

```c++
const Tensor *GetInputTensor(size_t index) const
const Tensor *GetRequiredInputTensor(size_t ir_index) const
const Tensor *GetOptionalInputTensor(size_t ir_index) const
const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const
ge::graphStatus GetOption(const ge::AscendString &option_key, ge::AscendString &option) const
ge::graphStatus GetPlatformInfos(fe::PlatFormInfos &platform_info, fe::OptionalInfos& optional_infos) const
```
