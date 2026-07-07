# SetGraphOutput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置图输出。

## 函数原型

```c++
ge::Status SetGraphOutput(EsCTensorHolder *tensor, int64_t output_index)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | 张量持有者。 |
| output_index | 输入 | 输出索引。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::Status | 操作状态。 |

## 约束说明

无
