# Build

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher\_config.h\>
- 库文件：libge\_compiler.so

## 功能说明

构造PatternMatcherConfig对象的接口。

## 函数原型

```c++
std::unique_ptr<PatternMatcherConfig> Build() const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| std::unique_ptr\<PatternMatcherConfig> | 输出 | 匹配规则。 |

## 返回值说明

无

## 约束说明

无
