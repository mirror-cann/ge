# 简介

用于保存图的缓存数据。

## 需要包含的头文件

```c++
#include <graph/graph_buffer.h>
```

## Public成员函数

```c++
GraphBuffer()
GraphBuffer(const GraphBuffer &) = delete
GraphBuffer &operator=(const GraphBuffer &) = delete
~GraphBuffer()
const std::uint8_t *GetData() const
std::size_t GetSize() const
```
