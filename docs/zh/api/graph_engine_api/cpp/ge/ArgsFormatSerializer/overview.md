# 简介

用于对ArgsFormat进行序列化和反序列化。

## 需要包含的头文件

```c++
#include <graph/arg_desc_info.h>
```

## Public成员函数

```c++
static AscendString Serialize(const std::vector<ArgDescInfo> &args_format)
static std::vector<ArgDescInfo> Deserialize(const AscendString &args_str)
```
