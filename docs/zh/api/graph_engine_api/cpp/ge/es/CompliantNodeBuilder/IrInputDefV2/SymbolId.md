# SymbolId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输入定义的符号标识，用于动态shape推导或符号化表达，建立输入输出之间的符号关联关系。

## 函数原型

```c++
IrInputDefV2 &SymbolId(const char_t *symbol_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| symbol_id | 输入 | 输入tensor的符号标识，用于：<br><br>  - 动态shape场景：标识动态维度（如batch_size、height、width）。<br>  - 符号化推导：建立输入输出之间的符号关系（类似数学公式中的变量符号）。<br>  - shape关联：多个tensor共享相同符号时，可推导它们之间的shape关系。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrInputDefV2 & | IR输入定义类自身引用，支持链式调用。 |

## 约束说明

无
