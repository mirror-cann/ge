# 简介

构造PatternMatcherConfig的辅助类。

## 需要包含的头文件

```c++
#include <ge/fusion/pattern_matcher_config.h>
```

## Public成员函数

```c++
 PatternMatcherConfigBuilder()
~PatternMatcherConfigBuilder()
PatternMatcherConfigBuilder &EnableConstValueMatch()
PatternMatcherConfigBuilder &EnableIrAttrMatch()
std::unique_ptr<PatternMatcherConfig> Build() const
```
