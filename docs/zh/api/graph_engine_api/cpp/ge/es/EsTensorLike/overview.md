# 简介

泛EsTensorHolder类型，支持将标量和向量数值转为EsTensorHolder对象，便于C++构图接口可以直接传递数值来构图。

## 需要包含的头文件

```c++
#include <es_tensor_like.h>
```

## Public成员函数

```c++
EsTensorLike(const EsTensorHolder &tensor)
EsTensorLike(const std::nullptr_t)
EsTensorLike(const int64_t value)
EsTensorLike(const float value)
EsTensorLike(const std::vector<int64_t> &values)
EsTensorLike(const std::vector<float> &values)
~EsTensorLike()
EsCGraphBuilder *GetOwnerBuilder() const
EsTensorHolder ToTensorHolder(EsCGraphBuilder *graph) const
```

## 对外函数

```c++
EsCGraphBuilder *ResolveBuilderImpl(const EsTensorLike &tensor_like)
EsCGraphBuilder *ResolveBuilderImpl(const std::vector<EsTensorHolder> &tensors)
EsCGraphBuilder *ResolveBuilderImpl(EsCTensorHolder *esb_tensor)
EsCGraphBuilder *ResolveBuilderImpl(const std::vector<EsCTensorHolder *> &esb_tensors)
EsCGraphBuilder *ResolveBuilderImpl(const EsGraphBuilder *graph_builder)
EsCGraphBuilder *ResolveBuilderImpl(EsCGraphBuilder *graph_builder)
EsCGraphBuilder *ResolveBuilderImpl(const std::nullptr_t)
EsCGraphBuilder *ResolveBuilder()
EsCGraphBuilder *ResolveBuilder(const T& first, const Ts&... rest)
```
