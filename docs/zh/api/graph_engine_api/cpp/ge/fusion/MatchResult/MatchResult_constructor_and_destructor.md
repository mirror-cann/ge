# MatchResult构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/match\_result.h\>
- 库文件：libge\_compiler.so

## 功能说明

MatchResult构造函数和析构函数。

定义匹配结果，入参为Pattern即匹配定义。在MatchResult中，Matched Node为根据Pattern Node匹配到的节点，与Pattern Node一一对应。

## 函数原型

```c++
explicit MatchResult(const Pattern *pattern);
MatchResult(const MatchResult &other) noexcept;
MatchResult(MatchResult &&other) noexcept;
MatchResult &operator=(const MatchResult &other) noexcept;
MatchResult &operator=(MatchResult &&other) noexcept ;
~MatchResult();
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pattern | 输入 | Pattern定义。 |

## 返回值说明

无

## 异常处理

无

## 约束说明

无
