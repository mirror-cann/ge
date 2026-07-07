# 简介

用于存储Feature内存信息。

## 需要包含的头文件

```c++
#include <ge/ge_feature_memory.h>
```

## Public成员函数

```c++
~FeatureMemory()
FeatureMemory &operator=(const FeatureMemory &) & = delete
FeatureMemory(const FeatureMemory &) = delete
MemoryType GetType() const
size_t GetSize() const
bool IsFixed() const
```
