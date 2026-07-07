# PassRegistrationData构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

PassRegistrationData构造函数和析构函数。

## 函数原型

```c++
PassRegistrationData() = default
~PassRegistrationData() = default
PassRegistrationData(std::string pass_name)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pass_name | 输入 | 自定义Pass的名字。 |

## 返回值说明

PassRegistrationData构造函数返回PassRegistrationData类型的对象。

## 约束说明

无
