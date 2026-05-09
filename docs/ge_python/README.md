# GE-PY Python 模块文档

## 概述

GE-PY 是 GraphEngine 的 Python 接口模块，提供了 Pythonic 的图相关接口。为用户提供了便捷的图构建和操作，编译执行等功能。该模块对外头文件位于 `api/python/ge/ge/` 目录下。

GE-PY 模块包含以下核心组件：

- **graph 模块** - 图基础操作模块，提供 Graph、Node、Tensor 等核心类
- **passes 模块** - 自定义 Fusion Pass 开发模块，提供 Python 级别的图融合 Pass 开发能力
- **ge_global 模块** - GE 全局初始化和析构接口
- **offline_compile 模块** - 离线图编译接口
- **session 模块** - 图编译执行接口
- **allocator 模块** - 内存分配器抽象，供异步执行场景注册外置 allocator
- **es 模块** - Eager-Style 图构建接口，提供函数式风格的图构建方式
- **pyatc 模块** - `atc` 命令行等价入口，便于指定 ATC 进程内的 python 解释器

## 文档导航

### 设计文档

- **[GE-PY 模块类关系文档](design/ge_python.md)** - Graph、Node、Tensor、Session 等基础模块的详细说明
  - offline_compile 模块：离线图编译接口
  - Graph 类：图操作的主要接口
  - Node 类：图节点操作接口
  - Tensor 类：张量数据类
  - GeApi 类：GE 初始化和析构
  - Session 类：图编译执行接口
  - Allocator 类：异步执行场景下的外置内存分配器接口

- **[ES-PY 模块文档](../es/api/es_python.md)** - Eager-Style 图构建模块的详细说明
  - GraphBuilder 类：Eager-Style 图构建器
  - TensorHolder 类：张量持有者

- **[Python Pass 设计文档](design/ge_python_pass_design.md)** - Python 自定义 Fusion Pass 开发能力的详细设计说明
  - FusionBasePass 类：基础融合 Pass 基类
  - PatternFusionPass 类：模式匹配融合 Pass 基类
  - DecomposePass 类：算子分解 Pass 基类
  - Pass 注册与发现机制
  - Bridge 架构、native helper 与多版本产物装载设计

- **[pyatc CLI 设计文档](design/pyatc_cli_design.md)** - pyatc 命令行的详细设计说明

## 模块关系

- **graph 模块** - 提供图的基础操作能力，是其他模块的基础
- **passes 模块** - 提供自定义 Fusion Pass 开发能力，通过装饰器注册 Pass，在编译阶段由 GE 自动发现并执行
- **es 模块** - 提供函数式图构建方式，最终构建出 graph 模块的 Graph 对象
- **allocator 模块** - 为 session 异步执行提供按 stream 维度注册的外置内存分配能力
- **session 模块** - 使用 graph 模块构建的图进行编译和执行，编译过程中会加载并执行 passes 模块注册的 Pass
- **ge_global 模块** - 提供全局初始化和资源管理
- **offline_compile 模块** - 提供离线模型构建、导出能力
- **pyatc 模块** - 提供与 `atc` 等价的命令行入口，便于指定 ATC 进程内的 python 解释器

## 使用示例

### 基础图操作示例

参考 [使用es的python api构图sample](../../examples/es/transformer/python/README.md)的方式执行用例， 特别需要说明的是:
[需要先安装包并设置对应的环境变量](../../examples/es/transformer/python/README.md#31准备cann包)

### 离线图编译执行示例

参考 [使用offline_compile的python api离线图编译执行sample](../../examples/offline_compile_run/python/README.md) 的方式执行用例，特别需要说明的是：
[需要先安装包并设置对应的环境变量](../../examples/offline_compile_run/python/README.md#31准备cann包)

### 更多示例

更多 Python 用例请参考 [examples/es](../../examples/es) 目录下的各个子目录：

## 开发路线图

我们在2025年首次推出了`ge-python`的模块，目标是提供 Python 语言的构图、编译图、执行图的能力， 2026Q1 我们主要工作是重点完成 es api 集成，让用户安装好 ops 包后使用 Python 的 es api 构图能力：

---

### 核心架构

- [x] [***December 2025***] `ge-python` 模块已经完成设计和落地，具备了基本的使用es api 构图、 编译图、 执行图的能力。

### 基础API 集成

- [x] [***December 2025***]基础接口已经完成设计和落地。
- [x] [***February 2026***] es 的 python 算子 api 支持，详见[es api集成路标](../es/README.md#api-集成)。
- [x] [***April 2026***] 图异步执行的python接口提供
- [x] [***April 2026***] 离线图编译执行的python接口提供

### 自定义pass

- [~] [***April 2026***] 开发态主链已完成 `FusionBasePass` 与 `PatternFusionPass`，`DecomposePass` 待补齐
- [ ] [***后续阶段***] 预制版本、fallback、多版本 native artifact 与 `entry_points` 产品化待补齐

### sample和相关文档

- [x] [***December 2025***]已提供对应的sample，涵盖常见使用场景。
- [x] [***December 2025***]已提供细化的文档，即本目录。

### 后向兼容

- [x] [***December 2025***]Python api 后向兼容完成设计并落地。

### others
