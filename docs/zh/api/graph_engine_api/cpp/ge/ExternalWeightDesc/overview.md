# 简介

落盘的外置权重的详细信息。

## 需要包含的头文件

```c++
#include <ge/ge_external_weight_desc.h>
```

## Public成员函数

```c++
~ExternalWeightDesc();
ExternalWeightDesc &operator=(const ExternalWeightDesc &) & = delete;
ExternalWeightDesc(const ExternalWeightDesc &) = delete;
AscendString GetLocation() const;
size_t GetSize() const;
size_t GetOffset() const;
AscendString GetId() const;
```
