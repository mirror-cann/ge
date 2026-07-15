## 融合Pass样例

本目录提供了继承GE提供的类并重写其方法来实现自定义融合pass的相关样例：

| 样例                                         | 样例链接                                                      |
|--------------------------------------------|-----------------------------------------------------------|
| MatMul+Add融合为GEMM自定义pass样例                 | [README](1_fuse_matmul_add_pass/README.md)                |
| 移动Concat后ReLu至Concat前的自定义pass样例            | [README](2_move_relu_before_concat_pass/cpp/README.md)    |
| 移动Concat后ReLu至Concat前的自定义pass样例（Python 版本） | [README](2_move_relu_before_concat_pass/python/README.md) |
| 修改卷积算子data format的自定义pass样例                | [README](3_modify_conv_data_format_pass/cpp/README.md)        |
| 修改卷积算子data format的自定义pass样例（Python 版本）     | [README](3_modify_conv_data_format_pass/python/README.md)        |
| MMOE网络MatMul融合（Pack+BatchMatMulV2+Split）自定义pass样例 | [README](4_mmoe_bmm_split_pass/cpp/README.md)                  |
| MMOE网络首层MatMul融合（Concat+MatMul+SplitV）自定义pass样例 | [README](5_mmoe_matmul_pass/cpp/README.md)                     |

## 开发指南

如果要开发基于 pattern 匹配的融合 pass，推荐先阅读：

- [融合 Pattern Pass 机制](../../../docs/zh/design/features/fusion_pattern_pass.md)
- [Python 融合 Pass 开发指南](../python_fusion_pass_development_guide.md)
- [C++ 融合 Pass 开发指南](../cpp_fusion_pass_development_guide.md)
