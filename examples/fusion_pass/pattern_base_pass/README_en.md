# Fusion Pass Examples

This directory provides examples for implementing custom fusion passes by inheriting GE-provided classes and overriding their methods:

| Example | Example Link |
|---------|--------------|
| MatMul+Add fused to GEMM custom pass example | [README](1_fuse_matmul_add_pass/cpp/README.md) |
| MatMul+Add fused to GEMM custom pass example (Python version) | [README](1_fuse_matmul_add_pass/python/README.md) |
| capture tensor feature usage | [README](2_fuse_matmul_add_pass_with_capture_tensor/cpp/README.md) |
| capture tensor feature usage (Python version) | [README](2_fuse_matmul_add_pass_with_capture_tensor/python/README.md) |
| PatternMatcherConfig feature usage | [README](3_fuse_matmul_add_pass_with_pattern_matcher_config/cpp/README.md) |
| PatternMatcherConfig feature usage (Python version) | [README](3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md) |
| Custom pass example for deleting add zero operation | [README](4_add_zero_pass/cpp/README.md) |
| Custom pass example for deleting add zero operation (Python version, demonstrating `@pattern` expression syntax) | [README](4_add_zero_pass/python/README.md) |
| Custom pass example for custom operators | [README](5_add_zero_pass_in_custom_op/cpp/README.md) |
| Custom pass example for custom operators (Python version) | [README](5_add_zero_pass_in_custom_op/python/README.md) |
| Custom pass example for splitting grouped convolution | [README](6_decompose_grouped_conv_to_splited_pass/cpp/README.md) |
| Custom pass example for splitting grouped convolution (Python version) | [README](6_decompose_grouped_conv_to_splited_pass/python/README.md) |
| Custom pass example for flattening BatchMatMul to MatMul | [README](7_batch_matmul_flatten_pass/cpp/README.md) |

## Development Guide

Recommended to first read mechanism description, then choose language guide:

- [Fusion Pattern Pass Mechanism](../../../docs/zh/design/features/fusion_pattern_pass.md)
- [Python Fusion Pass Development Guide](../python_fusion_pass_development_guide.md)
- [C++ Fusion Pass Development Guide](../cpp_fusion_pass_development_guide.md)
