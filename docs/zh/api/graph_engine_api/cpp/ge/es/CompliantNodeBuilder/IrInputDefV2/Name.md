# Name

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

设置输入定义的名称，用于标识节点的输入端口名称。

## 函数原型

```c++
IrInputDefV2 &Name(const char_t *name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 输入端口名称，标识该输入在算子中的位置。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | IrInputDefV2 & | IR输入定义类自身引用，支持链式调用。 |

## 约束说明

无
