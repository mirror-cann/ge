# GE (Graph Engine)

## Latest News

- [2026/03] Continuously enhanced graph optimization capabilities, improved unit test coverage, and fixed compilation issues in multiple edge cases. Optimized documentation structure to enhance developer experience.
- [2026/02] Optimized auto-fusion and broadcast scenario support, added BF16 data type support. Enhanced HostCPU engine capabilities, optimized session creation and destruction lock mechanism. Fixed bugs in external weight and thread loading scenarios. Supported Reduce axis splitting Store address conflict penalty to improve operator fusion effects.
- [2026/01] GE project first release, open-sourced graph compiler and executor, supporting PyTorch and TensorFlow frontend integration and ONNX and PB model format parsing and compilation.

## Overview

GE (Graph Engine) is a graph compiler and executor for Ascend, providing computation graph optimization, multi-stream parallelism, memory reuse, and model sinking technologies to accelerate model execution efficiency and reduce model memory footprint.

GE provides friendly integration capabilities for PyTorch and TensorFlow frontends, and simultaneously supports parsing and compilation of mainstream model formats such as onnx and pb. See [Ascend Community Documentation - Graph Mode Development Guide](https://hiascend.com/document/redirect/CannCommunityGraphguide).

![](docs/zh/figures/architecture.png)

## Quick Start

To quickly experience GE's working methods and basic development process, refer to the following documentation:

- [Build Verification](docs/zh/build.md): Introduces the complete build process of components and test case execution flow.
- [Quick Start](examples/acl/1_sample_resnet50_imagenet_classification/README.md): Using ResNet50 model as an example, introduces how to use the ATC tool for model conversion and execute inference on Ascend AI processors.
- [Quick Start-LLM](examples/acl/3_sample_qwen_llm/README.md): Using Qwen model as an example, introduces how to use the ATC tool for LLM model conversion and implement LLM model loading, execution, and result retrieval.

## Documentation

To learn how to use GE for model compilation and execution, refer to the Graph Mode Development Guide, technical articles, and other content:  [GE Reference Materials](docs/README.md)

To gain a deep understanding of GE's internal design, architecture mechanisms, and development processes, refer to the following documentation:

* [GE Architecture Documentation](docs/zh/design/architecture.md): Introduces core components, execution flows, optimization mechanisms, and other internal principles.
* [Contributing Guide](CONTRIBUTING.md): Explains how to submit Issues, Pull Requests, and code standards.
* [AI Agent Support](.opencode/README.md): Introduces some default skills used in the repository and using agents to assist the development process.

## Ecosystem Integration

The following projects have integrated GE as an inference or graph mode backend:

- **TorchAir**: Integrates GE into PyTorch graph mode. [Link](https://gitcode.com/Ascend/torchair)
- **TFA (TensorFlow Adapter)**: Uses GE as TensorFlow backend. [Link](https://gitcode.com/cann/tensorflow)
- **JittorInfer**: Large model C++ inference framework based on Ascend chips. [Link](https://github.com/Jittor/JittorInfer)
- **Triton GE Backend**: GE's Triton Inference Server backend. [Link](https://gitcode.com/cann/triton-inference-server-ge-backend)

**Note:** The above list shows known and publicly disclosed integration cases, **not a complete list**. If you are using GE, welcome to supplement through Issues or Pull Requests, and we will continuously update relevant information.

## Other Information

- [Security Statement](SECURITY.md)
- [License](LICENSE)

## Contact Us

<img src="./docs/zh/figures/GE_contact.png" width="400">
