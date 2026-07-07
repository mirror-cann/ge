# 构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/op\_lib\_register.h\>
- 库文件：libregister.so

## 功能说明

OpLibRegister的构造函数和析构函数。

## 函数原型

```c++
explicit OpLibRegister(const char_t *vendor_name)
OpLibRegister(OpLibRegister &&other) noexcept
OpLibRegister(const OpLibRegister &other)
OpLibRegister &operator=(const OpLibRegister &) = delete
OpLibRegister &operator=(OpLibRegister &&) = delete
~OpLibRegister()
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| vendor_name | 输入 | 自定义算子厂商名。 |
| other | 输入 | 另一个OpLibRegister对象。 |

## 返回值说明

返回一个OpLibRegister对象。

## 约束说明

无
