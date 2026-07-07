# SymbolId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输出的符号标识，用于动态shape推导。

## 函数原型

```c++
IrOutputDefV2 &SymbolId(const char_t *symbol_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| symbol_id | 输入 | 输出tensor的符号标识，建立输出与输入之间的shape关联。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrOutputDefV2 & | IR输出定义类自身引用，支持链式调用。 |

## 约束说明

无
