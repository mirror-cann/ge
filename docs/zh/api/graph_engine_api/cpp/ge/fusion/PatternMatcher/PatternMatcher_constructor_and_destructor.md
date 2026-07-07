# PatternMatcher构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher.h\>
- 库文件：libge\_compiler.so

## 功能说明

通过Pattern和Target Graph构造一个PatternMatcher。

## 函数原型

```c++
explicit PatternMatcher(std::unique_ptr<Pattern> pattern, const GraphPtr &target_graph)
~PatternMatcher()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| pattern | 输入 | Pattern定义。构造完PatternMatcher后其生命周期由Matcher管理。 |
| target_graph | 输入 | 待匹配的Graph。 |

## 返回值说明

无

## 约束说明

无
