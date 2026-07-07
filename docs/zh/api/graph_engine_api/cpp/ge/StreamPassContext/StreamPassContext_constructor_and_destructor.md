# StreamPassContext构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

StreamPassContext构造函数和析构函数。

## 函数原型

```c++
explicit StreamPassContext(int64_t current_max_stream_id)
~StreamPassContext() override = default
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| current_max_stream_id | 输入 | 当前图中最大的Stream ID。 |

## 返回值说明

无

## 约束说明

无
