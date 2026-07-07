# SetOriginSymbolShape

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_c\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置原始符号形状。

## 函数原型

```c++
ge::Status SetOriginSymbolShape(const char *const *shape_str, const int64_t shape_str_num)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| shape_str | 输入 | 形状字符串数组。 |
| shape_str_num | 输入 | 形状字符串数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::Status | 操作状态。<br><br>  - 0：成功<br>  - 1：失败 |

## 约束说明

无
