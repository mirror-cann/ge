# 自定义融合 Pass

本目录提供 GE 自定义融合 pass 的开发文档和样例。

如果是第一次开发，建议按下面顺序阅读：

1. [融合 Pattern Pass 机制](../../docs/architecture/features/fusion_pattern_pass.md)：先理解 pattern、匹配、过滤、replacement 和边界规则。
2. [Python 融合 Pass 开发指南](python_fusion_pass_development_guide.md)：支持运行时接入和 `@pattern` 表达式写法。
3. [C++ 融合 Pass 开发指南](cpp_fusion_pass_development_guide.md)：适合编译成 `.so` 后产品化交付。

## 样例目录

| 目录 | 说明 |
|------|------|
| [pattern_base_pass](pattern_base_pass/README.md) | 推荐优先参考。通过 `PatternFusionPass` 或 `DecomposePass` 开发 pattern 类融合规则 |
| [graph_base_pass](graph_base_pass/README.md) | 通过 graph 接口直接改图的样例，适合需要完全手动控制图修改的场景 |
