# AppendGraphInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_graph\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

在图的末尾添加图输入。

## 函数原型

```c++
EsCTensorHolder *AppendGraphInput(const ge::char_t *name = nullptr, const ge::char_t *type = nullptr)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输入名称，可选。 |
| type | 输入 | 输入类型，可选。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | EsCTensorHolder * | 张量持有者指针。 |

## 约束说明

无
