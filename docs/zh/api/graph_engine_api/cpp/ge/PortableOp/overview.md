# 简介

自定义算子支持离线推理场景接口，用于序列化和反序列化自定义算子数据。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
virtual graphStatus Serialize(std::vector<uint8_t> &buffer) = 0
virtual graphStatus Deserialize(const std::vector<uint8_t> &buffer) = 0
```
