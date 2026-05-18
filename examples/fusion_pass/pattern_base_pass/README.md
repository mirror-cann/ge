## 融合Pass样例

本目录提供了继承GE提供的类并重写其方法来实现自定义融合pass的相关样例：

| 样例                              | 样例链接                                                                     |
|---------------------------------|--------------------------------------------------------------------------|
| MatMul+Add融合为GEMM自定义pass样例      | [README](1_fuse_matmul_add_pass/cpp/README.md)                           |
| MatMul+Add融合为GEMM自定义pass样例（Python版本） | [README](1_fuse_matmul_add_pass/python/README.md)                        |
| capture tensor 功能的使用            | [README](2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md)       |
| capture tensor 功能的使用（Python版本）            | [README](2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)    |
| PatternMatcherConfig 功能的使用      | [README](3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md) |
| PatternMatcherConfig 功能的使用（Python版本）       | [README](3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md) |
| 删除加零操作的自定义pass样例                | [README](4_add_zero_pass/cpp/README.md)                                  |
| 删除加零操作的自定义pass样例（Python版本）     | [README](4_add_zero_pass/python/README.md)                               |
| 自定义算子的自定义pass样例                 | [README](5_add_zero_pass_in_custom_op/cpp/README.md)                       |
| 自定义算子的自定义pass样例（Python 版本）     | [README](5_add_zero_pass_in_custom_op/python/README.md)                  |
| 拆分分组卷积的自定义pass样例                | [README](6_decompose_grouped_conv_to_splited_pass/cpp/README.md)         |
| 拆分分组卷积的自定义pass样例（Python版本）     | [README](6_decompose_grouped_conv_to_splited_pass/python/README.md)      |

## 开发指南

更多关于融合Pass开发的信息，请参考：[融合Pass开发指南](../融合Pass开发指南.md)
