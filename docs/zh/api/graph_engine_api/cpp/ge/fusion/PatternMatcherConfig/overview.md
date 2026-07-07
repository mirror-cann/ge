# 简介

用于配置PatternMatcher行为的类。

请使用[PatternMatcherConfigBuilder](../PatternMatcherConfigBuilder/PatternMatcherConfigBuilder.md)来构造PatternMatcherConfig。

## 需要包含的头文件

```c++
#include <ge/fusion/pattern_matcher_config.h>
```

## Public成员函数

```c++
PatternMatcherConfig()
~PatternMatcherConfig()
PatternMatcherConfig(const PatternMatcherConfig &other)
PatternMatcherConfig &operator=(const PatternMatcherConfig &other)
PatternMatcherConfig(PatternMatcherConfig &&other) noexcept
PatternMatcherConfig &operator=(PatternMatcherConfig &&other) noexcept
bool IsEnableConstValueMatch() const
bool IsEnableIrAttrMatch() const
```
