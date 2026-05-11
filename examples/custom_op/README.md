# 自定义算子入图样例

本目录提供自定义算子入图相关样例，覆盖不同构图入口、算子编成语言和模型下沉链路。

## 样例总览

| 样例 | 场景 | 构图入口 | 算子编成语言 | 编译方式 | 模型下沉能力 | 链接 |
| --- | --- | --- | --- | --- | --- | --- |
| `ascendc_add_custom` | Ascend C 算子通过 GE 入图 | PyTorch + TorchAir | Ascend C | 预编译/直接调用 | 不涉及 | [README](./ascendc_add_custom/README.md) |
| `triton_add_custom` | Triton 算子通过 GE 入图 | TensorFlow         | Triton | 预编译为 `npubin` | 不涉及 | [README](./triton_add_custom/README.md) |
| `compilable_add_custom` | Ascend C 算子通过 GE 入图并生成 om离线模型 | GE构图 + ATC离线编译 | Ascend C | RTC算子运行时编译 | 支持模型下沉到 om离线模型 | [README](./compilable_add_custom/README.md) |

## 如何选择样例

- 想看 `PyTorch/TorchAir + Ascend C` 的入图方式，参考 `ascendc_add_custom`
- 想看 `GE构图 + ATC离线编译 + om离线模型` 链路中的 Ascend C 自定义算子，参考 `compilable_add_custom`
- 想看 `TensorFlow + Triton` 的入图方式，参考 `triton_add_custom`

## 推荐阅读顺序

1. 如果是第一次看 `custom_op` 目录，建议先读 `ascendc_add_custom`，先理解 Ascend C 算子通过 GE 入图的最基础链路。
2. 再读 `triton_add_custom`，补齐另一类自定义算子通过 GE 入图的基本形态和目录组织方式。
3. 最后读 `compilable_add_custom`，它可以看作是在前两个基础用例都看过之后再阅读的扩展示例，进一步叠加了 `CompilableOp/PortableOp`、RTC算子运行时编译、`GE构图 + ATC离线编译` 和 `om离线模型` 下沉链路。
4. 进入具体 sample 后，优先阅读 README 里的样例概述、前置依赖、快速运行、结果校验，再看关键文件说明和附录。
