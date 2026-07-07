# 简介

用于做子图匹配的工具类，该类接收一个Pattern定义和待匹配的Graph。将使用Pattern对Graph进行匹配，提供MatchNext接口逐一返回匹配结果。

## 需要包含的头文件

```c++
#include <ge/fusion/pattern_matcher.h>
```

## Public成员函数

```c++
explicit PatternMatcher(std::unique_ptr<Pattern> pattern, const GraphPtr &target_graph)
[[nodiscard]] std::unique_ptr<MatchResult> MatchNext()
```
