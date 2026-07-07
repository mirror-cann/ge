# GetName

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

获取输入定义的名称，返回之前通过[Name](Name.md)设置的输入端口名称。

## 函数原型

```c++
const char_t *GetName() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | const char_t * | 返回输入端口名称的字符串指针，字符串内容与[Name](Name.md)设置的值相同。 |

## 约束说明

无
