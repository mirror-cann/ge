# SetErrorMessage

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <register/register\_custom\_pass.h\>
- 库文件：libregister.so

## 功能说明

向框架注册图修改阶段遇到的错误信息。

## 函数原型

```c++
void SetErrorMessage(const AscendString &error_message)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| error_message | 输入 | 要向框架注册的错误信息。 |

## 返回值说明

无

## 约束说明

无
