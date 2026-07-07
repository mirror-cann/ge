# 简介

逻辑流分配Pass上下文对象。

## 需要包含的头文件

```c++
#include <register/register_custom_pass.h>
```

## Public成员函数

```c++
explicit StreamPassContext(int64_t current_max_stream_id)
~StreamPassContext() override = default
int64_t AllocateNextStreamId()
int64_t GetCurrMaxStreamId() const
int64_t GetStreamId(const GNode &node) const
graphStatus SetStreamId(const GNode &node, int64_t stream_id)
```
