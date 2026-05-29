# 自定义算子入图样例

本目录提供自定义算子入图相关样例，覆盖不同构图入口、算子编成语言和模型下沉链路。

## 样例总览

| 样例 | 场景                          | 构图入口 | 算子编成语言 | 算子编译方式        | 模型下沉能力 | 链接 |
| --- |-----------------------------| -- | --- |---------------| --- | --- |
| `ascendc_add_custom` | Ascend C 算子通过 GE 入图         | PyTorch + TorchAir | Ascend C | CMake编译       | 不涉及 | [README](./ascendc_add_custom/README.md) |
| `triton_add_custom` | Triton 算子通过 GE 入图           | TensorFlow    | Triton | 预编译为 `npubin` | 不涉及 | [README](./triton_add_custom/README.md) |
| `compilable_add_custom` | Ascend C 算子通过 GE 入图并生成 om离线模型 | GE + ATC离线编译 | Ascend C | RTC算子运行时编译    | 支持模型下沉到 om离线模型 | [README](./compilable_add_custom/README.md) |
| `data_dependent_shape_custom` | 数据依赖 shape 算子             | GE | Ascend C | CMake编译       | 不涉及 | [README](data_dependent_shape_custom/README.md) |

## 通用开发流程

### 1. 编写自定义算子交付件

自定义算子交付件通常是一个可被 GE / 框架插件加载的 `.so`，核心是实现 `inc/graph_metadef/external/graph/custom_op.h` 中的能力接口，并通过 `REG_AUTO_MAPPING_OP` 注册。
GE 原生构图场景还需要提供 `REG_OP` proto 头文件，描述算子的输入、输出和属性，供构图侧创建 op type 时使用。

当前提供接口功能用途：

| 接口 / 宏                 | 用途                                                           |
|------------------------|--------------------------------------------------------------|
| `class BaseCustomOp`   | 自定义算子能力接口的公共基类，用户实现类按需组合继承其他能力接口。                            |
| `class EagerExecuteOp` | 运行时执行能力，可获取输入 Tensor、申请输出 Tensor、申请 workspace 并发起 kernel 调用。 |
| `class ShapeInferOp`   | Shape / DataType 推导能力，用于在编译或构图阶段设置输出描述。                      |
| `class CompilableOp`   | 在线编译能力，适合在 GE/ATC 编译过程中读取输入元信息、编译 kernel 或准备编译产物。            |
| `class PortableOp`     | 序列化 / 反序列化能力，用于在 OM 保存和加载阶段序列化 / 反序列化自定义算子kernel bin。        |
| `REG_OP`               | 定义 GE 原生构图可见的算子 proto，通常随交付件复制到 `op_graph/include/`。         |
| `REG_AUTO_MAPPING_OP`  | 静态注册自定义算子类型和创建宏，GE 按算子类型创建对应实现类。                             |

接口组合按场景选择：

| 场景                     | 推荐实现                                                                  |
|------------------------|-----------------------------------------------------------------------|
| 动态图在线执行                | `EagerExecuteOp` + `ShapeInferOp(可选)`                                 |
| 动态图在线执行 + 算子在线编译       | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp(可选)`                |
| 静态图离线下沉OM模型执行 + 算子在线编译 | `EagerExecuteOp` + `CompilableOp` + `ShapeInferOp(可选)` + `PortableOp` |

交付件命名可以按样例自行选择，但需要保证算子类型、注册类名和构图侧使用的 op type 对齐。

### 2. 配置交付件路径

自定义算子交付件需要通过 `ASCEND_CUSTOM_OPP_PATH` 暴露给 GE / ATC / 框架插件。推荐按 OPP 包根目录组织：

```text
<custom_opp_root>/
├── op_graph
│   ├── include
│   │   └── xxx_custom.h
│   └── lib
│       └── <os>
│           └── <arch>
│               └── libxxx_custom_op.so
└── framework
    └── tensorflow
        └── npu_supported_ops.json  // Tensorflow入图时需要
```
GE构图
配置方式：

```bash
export ASCEND_CUSTOM_OPP_PATH="<custom_opp_root>:$ASCEND_CUSTOM_OPP_PATH"
```

`<os>/<arch>` 按运行环境选择，例如 `linux/x86_64`、`linux/aarch64`。离线保存 OM 时，如果模型需要携带自定义算子 so，GE会基于运行环境读取 `<custom_opp_root>/op_graph/lib/<os>/<arch>/` 下的 `.so` 交付件。

### 3. 构图和前端接入

GE 原生构图时，构图侧需要能看到 `REG_OP` proto 头文件，并在图中创建与 `REG_AUTO_MAPPING_OP` 注册名一致的 op type。参考 `compilable_add_custom` 和 `data_dependent_shape_custom`。

PyTorch / TorchAir 入图时，除了 GE 侧自定义算子 `.so`，还需要 Python / PyTorch 侧的注册和转换逻辑：

| 前端 | 额外交付件 / 配置 |
| --- | --- |
| PyTorch + TorchAir | 需要 `TORCH_LIBRARY` / `TORCH_LIBRARY_IMPL` 注册 Python 可见算子，并通过 TorchAir converter 将 PyTorch 节点映射到 GE 自定义算子。 |
| TensorFlow | 需要 TensorFlow 侧自定义算子 `.so`，并提供框架插件可识别的 `npu_supported_ops.json`；TensorFlow Adapter 处理构图后会带入 GE 侧 `REG_OP` 信息。 |

不同前端的“入图”职责不同，但最终都需要让 GE 图里的 op type 能映射到自定义算子实现类。

### 4. 编译和运行

常见运行方式：

| 方式 | 说明                              | 接口要求 |
| --- |---------------------------------| --- |
| 在线 / 直接执行 | 进程内构图后直接执行，或框架图模式运行时执行。         | 通常需要 `EagerExecuteOp`，并按需实现 `ShapeInferOp`。 |
| 离线 OM | 构图后经 ATC 生成 离线OM模型，再由 ACL 加载执行。 | 需要 `PortableOp` 将编译产物序列化进 OM，并在执行阶段反序列化恢复。 |

如果只是框架在线图模式执行，可以不实现 `PortableOp`。如果目标是 `AIR -> ATC -> OM -> ACL` 的离线模型链路，则需要考虑编译产物如何随模型保存和恢复。

### 5. 开发检查项

- 算子类型名、注册类名、构图侧 op type 保持一致。
- kernel bin、源码来源明确，路径不要依赖临时工作目录。
- `ASCEND_CUSTOM_OPP_PATH` 指向 OPP 包根目录，而不是随意指向某个 `.so` 所在目录，除非对应 sample 明确采用简化目录。
- TensorFlow / PyTorch 入图场景同时检查框架侧交付件和 GE 侧交付件是否都已加载。
