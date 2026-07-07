# PatternMatcherConfig构造和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher\_config.h\>
- 库文件：libge\_compiler.so

## 功能说明

PatternMatcherConfig的构造函数。

## 函数原型

```c++
PatternMatcherConfig()
```

## 参数说明

无

## 返回值说明

无

## 约束说明

通过该构造函数创建的对象，其所有成员功能默认处于未启用状态。请使用[PatternMatcherConfigBuilder](../PatternMatcherConfigBuilder/PatternMatcherConfigBuilder.md)来构造。
