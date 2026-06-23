# Fusion Pass Examples

This directory provides examples for implementing custom fusion passes by inheriting GE-provided classes and overriding their methods:

| Example | Example Link |
|---------|--------------|
| MatMul+Add fused to GEMM custom pass example | [README](1_fuse_matmul_add_pass/README.md) |
| Move ReLU after Concat before Concat custom pass example | [README](2_move_relu_before_concat_pass/cpp/README.md) |
| Move ReLU after Concat before Concat custom pass example (Python version) | [README](2_move_relu_before_concat_pass/python/README.md) |
| Modify Conv operator data format custom pass example | [README](3_modify_conv_data_format_pass/cpp/README.md) |
| Modify Conv operator data format custom pass example (Python version) | [README](3_modify_conv_data_format_pass/python/README.md) |

## Development Guide

If developing pattern-based fusion passes, recommended reading:

- [Fusion Pattern Pass Mechanism](../../../docs/zh/design/features/fusion_pattern_pass.md)
- [Python Fusion Pass Development Guide](../python_fusion_pass_development_guide.md)
- [C++ Fusion Pass Development Guide](../cpp_fusion_pass_development_guide.md)
