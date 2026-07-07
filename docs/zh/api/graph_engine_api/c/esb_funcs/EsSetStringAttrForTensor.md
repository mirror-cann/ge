# EsSetStringAttrForTensor

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置Tensor的字符串类型属性。

## 函数原型

```c
uint32_t EsSetStringAttrForTensor(EsCTensorHolder *tensor, const char *attr_name, const char *value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | Tensor持有者指针。 |
| attr_name | 输入 | 属性名称。 |
| value | 输入 | 属性值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | uint32_t | 成功为0，失败返回非0值。 |

## 约束说明

无
