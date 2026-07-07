# GetOptionValue

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

通过option的key，从上下文中获取option的值。

## 函数原型

```c++
graphStatus GetOptionValue(const AscendString &option_key, AscendString &option_value) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| option_key | 输入 | option的key。 |
| option_value | 输出 | option的值。 |

## 返回值说明

状态码，若option key不存在，则返回失败。

## 约束说明

无
