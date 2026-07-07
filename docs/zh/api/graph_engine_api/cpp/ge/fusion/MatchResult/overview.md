# 简介

通过[PatternMatcher](../PatternMatcher/PatternMatcher.md)在图上匹配到的一个结果，该结果包含匹配到的node。

## 需要包含的头文件

```c++
#include <ge/fusion/match_result.h>
```

## Public成员函数

```c++
MatchResult()
explicit MatchResult(const Pattern *pattern)
MatchResult(const MatchResult &other) noexcept
MatchResult(MatchResult &&other) noexcept
MatchResult &operator=(const MatchResult &other) noexcept
MatchResult &operator=(MatchResult &&other) noexcept
Status AppendNodeMatchPair(const NodeIo &pattern_node_out,const NodeIo &matched_node_out)
Status GetMatchedNode(const GNode &pattern_node, GNode &matched_node) const
[[nodiscard]] std::vector<GNode> GetMatchedNodes() const
Status GetCapturedTensor(size_t capture_idx, NodeIo &node_output) const
[[nodiscard]] std::unique_ptr<SubgraphBoundary> ToSubgraphBoundary() const
[[nodiscard]] AscendString ToAscendString() const
```
