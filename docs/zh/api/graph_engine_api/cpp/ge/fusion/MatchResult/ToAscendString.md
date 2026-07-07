# ToAscendString

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

将Match Result序列化为字符串。

## 函数原型

```c++
AscendString ToAscendString() const
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | AscendString | 以字符串的形式描述Match Result的Pattern name，及Pattern Node和Match Node name的对应关系。 |

## 约束说明

无
