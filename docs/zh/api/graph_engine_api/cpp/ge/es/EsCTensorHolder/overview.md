# 简介

表示ES API的C风格的输出类型；C用户构图调用esb\_funcs.h接口即可，C++用户调用EsTensorHolder的类的对外接口即可。

## 需要包含的头文件

```c++
#include <es_c_tensor_holder.h>
```

## Public成员函数

```c++
EsCTensorHolder(EsCGraphBuilder &owner, const ge::GNode &producer, int32_t index)
~EsCTensorHolder()
EsCTensorHolder(EsCTensorHolder &&) noexcept
EsCTensorHolder &operator=(EsCTensorHolder &&) noexcept
ge::Status SetDataType(const ge::DataType data_type)
ge::Status SetFormat(const ge::Format format)
ge::Status SetOriginFormat(const ge::Format format)
ge::Status SetStorageFormat(const ge::Format format)
ge::Status SetOriginShape(const ge::Shape &shape)
ge::Status SetStorageShape(const ge::Shape &shape)
ge::Status SetShape(const ge::Shape &shape)
ge::Status SetOriginSymbolShape(const char *const *shape_str, const int64_t shape_str_num)
ge::GNode &GetProducer()
int32_t GetOutIndex() const
EsCGraphBuilder &GetOwnerBuilder()
```
