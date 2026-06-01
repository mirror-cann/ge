# GE Pass Python化 V1 设计文档

## 1. 背景

当前 GE 已具备两类与本需求直接相关的基础能力：

- 现有自定义 pass 装载链路已经存在，GE 会通过 `opp/vendors/*/custom_fusion_passes/*.so` 的方式发现并 `dlopen` pass 库。
- Python 侧已经具备 `ge.es` 构图能力与 `ge.graph` 基础图接口。

本设计的目标是在不推翻现有 GE pass 执行框架的前提下，引入正式的 Python pass 开发能力，使用户既可以快速本地开发和验证，也可以将 pass 以标准 Python 包的形式进行团队分发。

## 2. 目标与范围

### 2.1 目标

- 支持用户使用 Python 开发 GE pass。
- 复用现有 GE pass 执行链路，不新增第二套 pass 调度框架。
- 复用已有 `ge.es` Python 构图能力，不重新设计 Python ES。
- 首先支持环境变量驱动的开发态接入，后续再补发布态自动发现。
- 为后续 Python ATC 入口复用同一套 pass 注册和发现协议预留扩展点。

### 2.2 V1 范围

- 支持三类 pass：
- `FusionBasePass`
- `PatternFusionPass`
- `DecomposePass`

- bring-up 与独立 bridge 拆分阶段仍以 `FusionBasePass` 回归作为最小验证链路；正式 wrapper 化后的首个完整验收目标转为 `PatternFusionPass`，`DecomposePass` 在其后补齐，但整体仍属于本设计文档的 V1 范围。

- 当前阶段发现机制先收敛为：
- 环境变量 `ASCEND_GE_PY_PASS_PATH`

- 后续阶段再补：
- `entry_points(group="ge.passes.plugins")`

- 当前代码已先收敛到环境变量主路径，`entry_points` 作为后续阶段能力补齐。

- 补齐 Python pass 编写所需的最小图接口能力。
- 当前仓直接产出 `ge_py` 主 wheel 和多版本 native 子 wheel。
- `examples/fusion_pass` 下现有 9 个 sample 规划提供 Python 对照版本。
- `REGISTER_CUSTOM_PASS` 需要支持，但不作为首批主链路目标，放在后续扩展阶段基于同一套 bridge / registry / session 机制复用实现。
- 当前优先整改项：
  - 发现机制先收敛到环境变量 `ASCEND_GE_PY_PASS_PATH`
  - `entry_points` 后续补齐
  - `python_pass_bootstrap_test.py` 迁移到 `tests/ge/ut/ge/graph/pyge_tests/` 并接入当前前端脚本
  - 新增文件年份统一使用 `2026`，现有老文件不做批量改年份
  - 第一优先级要以“`FusionBasePass` 正式 sample 通过环境变量方式端到端跑通”为目标，凡是这个目标涉及的能力都要正式完成，不涉及的能力可后置
  - 阶段 2 收口后，`PassContext` / `MatchResult` / `Pattern` / `PatternMatcherConfig` 的正式 Python 形态统一直接依赖 `_ge_pass_native.so`，不再保留 bring-up 期兼容 shim

### 2.3 非目标

- V1 不强制用户将 pass 打成 whl。
- V1 首批不以 legacy `REGISTER_CUSTOM_PASS` 体系作为主要 Python 化对象，优先覆盖 `PassRegistry` 体系的三类 pass；但架构和文档需要预留后续接入能力。
- V1 不新建第二套 Python-only pass 执行器。

## 3. 用户体验设计

### 3.1 开发态

用户安装 CANN 提供的 `ge_py` 后，只需要写普通 `.py` 文件或普通 Python package，并通过环境变量告诉 GE/ATC：

- `ASCEND_GE_PY_PASS_PATH=/abs/path1:/abs/path2`

当前阶段先把这一路径打磨稳定，不要求用户自己写 wheel 打包逻辑，也不要求用户理解 `entry_points`。

### 3.2 发布态（后续阶段）

当用户需要团队共享、版本固化、自动发现时，可以把 pass 做成独立 Python 包，并声明：

- `entry_points = {"ge.passes.plugins": [...]}`

GE 在运行时自动发现已安装的 pass 插件包。

### 3.3 说明

当前阶段环境变量不是兜底，而是主路径：

- `ASCEND_GE_PY_PASS_PATH=/abs/path1:/abs/path2`

后续再补 `entry_points` 自动发现。

## 4. 总体架构

### 4.1 架构原则

- GE 在初始化阶段通过一个上层统一 loader 收口 pass 插件加载。
- legacy custom pass 继续沿用现有 `.so + dlopen` 机制。
- 从长期产品化和扩展性角度，Python pass bridge 应从一开始就按“独立内部 bridge `.so`”设计，而不是直接编入 `ge_compiler.so`。
- 这里仍然不采用“作为 pass 插件被 `custom_fusion_passes` 发现和装载的 bridge `.so`”方案；推荐的是一个由 GE 内部 loader 显式装载的私有 bridge `.so`。
- 设计目标是让 `ge_compiler.so` 尽量保持 Python ABI 中立，只保留稳定的 pass runtime、注册表和 adapter 协议；所有直接依赖 `Python.h` / `pybind11` / `libpython` 的逻辑都应收敛到可替换的独立 bridge `.so` 中。
- 这样后续无论走预编译还是 fallback codegen，替换目标都是独立 bridge `.so`，而不是 run 包中的 `ge_compiler.so`。
- Python 侧统一通过 `ge.passes.bootstrap` 管理插件发现、模块导入和注册表。
- C++ 侧只关心“得到可执行 pass descriptor 并注册到 PassRegistry”，不关心用户 Python 文件具体在哪里。

### 4.2 核心组件

- `PassPluginLoader`
- 位于 compiler 层的统一 pass 插件加载入口
- 内部统一调 legacy `CustomPassHelper::Load()` 和 Python pass 注册逻辑
- 保持“一个调用入口闭环”，同时不把 Python 逻辑反向塞进 `graph_metadef/register`

- `ge.passes.bootstrap`
- Python 侧统一发现入口
- 当前以环境变量发现为主，后续再补 `entry_points`

- `ge.passes.registry`
- Python 侧注册表
- 负责存放 pass 元数据、类对象、阶段、类型、附加参数

- `ge.passes._bridge`
- Python 与 C++ bridge 的协议层
- 负责把 Python 注册表对象规范化为 C++ 可消费的数据结构

- `_ge_pass_native`
- 由 `PYBIND11_MODULE` 导出的 Python helper 模块
- 只承载 `Graph` / `PassContext` / `MatchResult` 等 native-backed wrapper 与 helper
- 不承载 `FusionBasePass` / `PatternFusionPass` / `DecomposePass` 这些用户可继承的 pass 基类

- C++ pass adapter
- 为三类 Python pass 提供对应 C++ 适配类
- 在适配类中回调 Python 对象方法

- 作为 pass 插件被 `dlopen` 的独立 bridge `.so`
- 当前主方案已明确放弃
- 文档中凡是基于“新增一个被 `custom_fusion_passes` 发现的 bridge `.so`”展开的描述，均应理解为“统一 loader + 私有内部 bridge `.so`”，而不是新的 pass 插件发现链

- 私有内部 bridge `.so`
- 这是本设计推荐的正式方向，而不是可选优化项
- 它不是 pass 插件，不参与 `custom_fusion_passes` 发现；它承载完整的 Python 版本敏感 bridge 逻辑，包括解释器初始化、GIL、对象转换、异常翻译和 Python 回调
- 正式交付时它与 `_ge_pass_native.so` 组成同一套 bridge artifact set，二者都需要纳入预编译与 fallback 管理

### 4.3 Native 绑定策略

V1 采用“两层绑定”策略，而不是把所有 Python 接口一次性迁到同一种绑定方式：

- `ge.graph` / `ge.es`
- 继续复用仓内现有的 C wrapper + `ctypes` 路线
- 这样改动面最小，可以优先复用现有 Python 图接口与 eager-style 构图能力

- Python pass bridge / adapter 层
- 使用 `pybind11` 作为核心实现策略
- 原因是这部分需要更自然地处理 `MatchResult` 包装、Python 对象生命周期、异常翻译和 GIL 管理

- 版本发布策略
- 主 wheel 负责 Python 代码、发现逻辑和运行时选择逻辑
- `cp39-cp314` 提供预编译 `pybind11` native 子 wheel
- 未命中预编译版本时，运行时 fallback codegen 作为最后兜底

也就是说，V1 并不是“全量 pybind 化”，而是“图接口沿用现有 ctypes/C wrapper，pass bridge 采用 pybind11”。

### 4.4 pybind11 使用方式选择

`pybind11` 在本方案里有两种典型使用方式：

- embed 模式
- 由 C++ 进程初始化 Python 解释器、`import` Python 模块并回调 Python 对象

- extension 模式
- 通过 `PYBIND11_MODULE` 把 C++ 能力导出为 Python 可直接 `import` 的 native 模块

当前已存在的 `compiler/graph/fusion/pass/python_fusion_base_pass_pybind_bridge.cc` 采用的是 embed 模式，而不是 extension 模式。原因是：

- 当前主链的方向是“GE compiler 调 Python pass”，不是“Python 主动 import 一个 C++ pass 运行时再反向驱动 compiler”
- `FusionBasePass` 第一阶段只需要让 C++ 安全地创建 Python 对象并调用其 `run()`，不需要先把 C++ 基类暴露给 Python 继承
- embed 模式可以先复用现有 `ge.passes.bootstrap / registry / _bridge` 的纯 Python 组织方式，降低首批落地成本

因此当前 bridge 文件不会出现 `PYBIND11_MODULE` 宏，这不是缺失，而是有意选择的模式差异。

但从长期设计看，embed bridge 不应继续直接编入 `ge_compiler.so`。当前工作区已经把它拆到独立 `libge_python_pass_bridge.so`，这也是后续应持续保持的正式边界。更合理的形态是：

- `ge_compiler.so`
- 只持有稳定的 loader、descriptor / adapter 协议和最小 C/C++ 交互面

- 独立内部 bridge `.so`
- 采用 embed 或 extension 中更合适的具体实现
- 承担所有 Python 版本敏感的 native 逻辑

- `_ge_pass_native.so`
- 作为 Python 可直接 `import` 的 helper extension
- 为 Python 层提供 native-backed wrapper 与 helper，但不定义用户可继承的 pass 基类

需要进一步强调的是，当前方案有两条明确边界，不能混用：

- 用户 pass 定义层
- 继续保持纯 Python 形式，用户继承 `ge.passes.base.FusionBasePass` / `PatternFusionPass` / `DecomposePass`

- native helper / wrapper 层
- 由 `_ge_pass_native.so` 提供 `Graph` / `PassContext` / `MatchResult` 等 wrapper
- 由 `libge_python_pass_bridge.so` 提供 embed 路径的运行时桥接

也就是说，本方案不要求也不推荐把 C++ `FusionBasePass` / `PatternFusionPass` 基类通过 `PYBIND11_MODULE` 暴露给 Python 继承。

需要特别说明的是，Python 版本敏感并不是只有 `PYBIND11_MODULE` 才会存在。只要 native 代码直接依赖：

- `Python.h`
- `pybind11`
- `libpython`

无论是 embed 还是 extension，都天然会和 Python minor version 产生二进制耦合。`PYBIND11_MODULE` 只是 extension 模式的导出入口，不是版本敏感的根因。

#### 4.4.1 `_ge_pass_native.so` 与 `libge_python_pass_bridge.so` 的装载位置差异

虽然这两个产物都属于 Python pass bridge artifact set，但它们处在两条不同的装载路径上：

- `_ge_pass_native.so`
  - 作为 Python extension module，由 Python 解释器通过 `import ge.passes._ge_pass_native` 装载
  - 它进入进程时，宿主解释器已经存在，因此在 Linux 上可以不把 `libpython.so` 显式写入 ELF `NEEDED`
  - 这类 extension 通常在 import 时直接向当前解释器进程解析 `Py_*` / `PyObject_*` 符号

- `libge_python_pass_bridge.so`
  - 作为 GE 内部 loader 视角的 embed bridge，由 `ge_compiler` 侧显式 `dlopen`
  - 它负责解释器初始化、解释器复用、GIL 管理、Python 模块导入和异常翻译
  - 因为它不能假设进程里已经存在 Python 解释器，所以需要显式链接 `libpython.so`

因此，“`_ge_pass_native.so` 在 ELF 中不显式依赖 `libpython.so`”只说明其装载上下文不同，并不说明它天然比 embed bridge 更容易跨版本复用。

#### 4.4.2 ABI 兼容边界：是否显式 `NEEDED libpython` 与是否受 Python 版本约束不是一回事

需要区分两个层面：

- ELF 层是否显式出现 `NEEDED libpythonX.Y.so`
- 该产物是否仍然绑定某个 CPython minor version 的 C API / ABI

对当前方案而言：

- `_ge_pass_native.so`
  - 即使不在 ELF 中显式携带 `libpython`
  - 仍然是基于当前 `HI_PYTHON` 对应的 `Python.h` / `pybind11` 头文件构建出来的 extension
  - 只要没有采用 `Py_LIMITED_API` / `abi3` 路线，它默认仍绑定具体的 CPython minor version ABI

- `libge_python_pass_bridge.so`
  - 因为采用 embed 路径，这种绑定会更直接地体现为显式 `NEEDED libpythonX.Y.so`
  - 其运行时约束也会更早、更明确地暴露出来

所以：

- `_ge_pass_native.so` 不显式 `NEEDED libpython`，不等于它能安全跨多个 Python minor version 复用
- `libge_python_pass_bridge.so` 的限制更显式、更硬，但二者在 ABI 意义上都属于 Python 版本敏感产物
- 如果未来需要扩大跨版本复用能力，方向应是评估 `abi3` / limited API，而不是仅凭“extension 不显式链接 `libpython`”来推断兼容性

结论上，这两个产物仍应被视为同一套 Python-version-sensitive artifact set，一起进入预编译与 fallback 管理。

#### 4.4.3 为什么 pass 基类继续保持纯 Python，而不是也迁到 native wrapper

当前方案刻意把“用户可继承的 pass 合同层”和“生命周期敏感的 native helper 层”分开：

- 用户可继承的 pass 合同层
  - `FusionBasePass`
  - `PatternFusionPass`
  - `DecomposePass`
  - 保持纯 Python

- 生命周期敏感的 native helper 层
  - `PassContext`
  - `MatchResult`
  - `Pattern`
  - `PatternMatcherConfig`
  - `release_graph()` 等 helper
  - 收敛到 `_ge_pass_native.so`

这样分层的原因是：

- pass 基类本质上是用户 DSL / contract
- 若把 `FusionBasePass` / `PatternFusionPass` 也迁到 `_ge_pass_native.so`
  - 会把用户侧 API 一并拉入 Python ABI 敏感面
  - 增加 import、发布和环境装配约束
  - 降低 Python 层协议、错误提示、类型约束和注册逻辑的可演进性

- 更重要的是，这样做并不能消除 `libge_python_pass_bridge.so`
  - 因为“GE 从 C++ 主动调 Python pass”这条 embed 链仍然存在
  - 也就是说，把 pass 基类 native 化只会扩大版本敏感面，而不会减少 bridge 的必要性

因此，本方案的正式边界仍然是：

- `libge_python_pass_bridge.so`
  - 负责 C++ 视角的 embed bridge

- `_ge_pass_native.so`
  - 负责 Python 视角的 native-backed wrapper 与 helper

- `ge.passes.base` / `registry` / `bootstrap` / `_bridge`
  - 继续承载用户 DSL、注册协议和桥接协议的纯 Python 部分

这也是为什么当前方案更推荐“基类纯 Python + helper native 化”，而不是“把 pass 基类也一起做成 wrapper 模块”。

另外，当前构建里的 `pybind_options` 只是一个 CMake `INTERFACE` 目标，用于屏蔽 pybind 头文件带来的部分编译告警，不承载运行时逻辑，也不是 pybind 的独立依赖产物。

## 5. 运行链路

### 5.1 初始化阶段

1. GE 启动并触发统一入口 `LoadPassPlugins()`
2. loader 内部先调用 legacy `CustomPassHelper::Load()`
3. loader 仅通过环境变量 `ASCEND_GE_PY_PASS_PATH` 判断是否需要拉起 Python pass 发现链路；`options` 不再承载 `ge.py_path / py_path` 这类用户 pass 路由参数
4. `ge_compiler.so` 内部的 bridge loader 调用 `RegisterPythonFusionBasePassesFromPlugin()`
5. bridge loader 通过 `dladdr + dlopen` 定位并装载同目录下的 `libge_python_pass_bridge.so`
6. bridge loader 通过 `GeGetPythonFusionBasePassBridgeApi()` 获取 bridge 导出的稳定入口，并把 registrar 回调传给 bridge
7. `libge_python_pass_bridge.so` 初始化 Python 运行时并导入 `ge.passes._bridge`
8. bridge 将当前进程环境中的 `ASCEND_GE_PY_PASS_PATH` 同步到 Python `os.environ`，再调用 `load_and_get_pass_descriptors()`
9. bootstrap 发现并导入用户 pass 模块
10. 用户 pass 模块通过装饰器把 pass 注册到 `ge.passes.registry`
11. bridge 读取注册表并通过 registrar 回调把 descriptor 动态注册回 `PassRegistry`

对应的调用关系可以简化为下面这条时序：

```text
PassPluginLoader / ge_compiler.so
  -> python_fusion_base_pass_bridge_loader.cc
  -> dlopen(libge_python_pass_bridge.so)
  -> GeGetPythonFusionBasePassBridgeApi()
  -> register_fusion_base_passes(registrar)
  -> ge.passes._bridge.load_and_get_pass_descriptors()
  -> ge.passes.bootstrap.load_pass_plugins()
  -> registrar.register_pass(pass_desc, callbacks)
  -> RegisterPythonFusionBasePass(...)
  -> PassRegistry / runtime registry
```

其中：

- `registrar` 由 loader 构造，代表“如何把 descriptor 注册回 compiler”
- `bridge` 负责发现 Python pass，并在拿到 descriptor 后回调 `registrar`
- `RegisterPythonFusionBasePass(...)` 是真正把 descriptor、callbacks 和 creator 挂回 compiler 侧注册中心的落点

阶段 1 拆分初期，`libge_python_pass_bridge.so` 可以先只依赖纯 Python `bootstrap / _bridge` 协议完成最小联调；阶段 2 收口后的正式方案则要求 `_ge_pass_native.so` 同步到位，bridge 与 Python API 默认基于同一套 native helper 运行。

### 5.2 执行阶段

1. `FusionPassExecutor` 按现有流程从 `PassRegistry` 获取 pass
2. 若 pass 为 Python pass，则实际创建的是对应的 C++ adapter 实例
3. adapter 在 `Run` 或相关阶段回调 Python pass 对象
4. Python pass 通过 `ge.graph` 与 `ge.es` 接口读写图和构建 replacement graph
5. 返回值映射为 GE `Status`

## 6. Python 公共接口设计

### 6.1 包结构

新增 `ge.passes` 包，提供以下公共接口：

- `FusionBasePass`
- `PatternFusionPass`
- `DecomposePass`
- `register_fusion_pass`
- `register_decompose_pass`
- `PassStage`
- `PassContext`
- `Pattern`
- `NodeIo`
- `MatchResult`
- `PatternMatcherConfig`
- `PatternMatcherConfigBuilder`
- `create_pattern`
- `create_replacement`
- `load_pass_plugins`
- `get_registered_passes`

### 6.2 方法风格

默认采用 Python 风格命名

### 6.3 注册接口

建议形态如下：

```python
from ge.passes import FusionBasePass, PassStage, register_fusion_pass

@register_fusion_pass(name="ConvFormatPass", stage=PassStage.BEFORE_INFER_SHAPE)
class ConvFormatPass(FusionBasePass):
    def run(self, graph, context):
        return 0
```

```python
from ge.passes import DecomposePass, PassStage, register_decompose_pass

@register_decompose_pass(
    name="DecomposeGroupedConv",
    stage=PassStage.AFTER_INFER_SHAPE,
    op_types=["Conv2D"],
)
class DecomposeGroupedConv(DecomposePass):
    def meet_requirements(self, node):
        return True

    def replacement(self, node):
        ...
```

### 6.4 Python 侧返回值约定

- `run` 返回 `StatusLike`
- `meet_requirements` 返回 `bool`
- `patterns` 返回 `list[Pattern | Graph]`
- `replacement` 返回 `Graph`
- 若希望跳过当前匹配，必须在 `meet_requirements` 返回 `False`，不支持通过 `replacement` 返回 `None` 表达“放弃替换”

其中 `StatusLike` 在 Python 层统一转换为 GE `Status`。

## 7. 发现机制设计

### 7.1 统一入口

Python 侧统一提供：

- `ge.passes.bootstrap.load_pass_plugins()`
- `ge.passes.bootstrap.get_registered_passes()`

bridge 注册链路在每轮加载前先由 C++ 按当前进程环境刷新 Python `os.environ` 中的 `ASCEND_GE_PY_PASS_PATH`，避免常驻 Python 解释器中的环境缓存影响下一轮 pass 发现。

### 7.2 发现优先级

当前阶段优先级收敛为：

1. 环境变量 `ASCEND_GE_PY_PASS_PATH`

后续阶段再补：

2. `entry_points(group="ge.passes.plugins")`

### 7.3 环境变量模式

- `ASCEND_GE_PY_PASS_PATH` 支持多个目录，以 `:` 分隔
- 目录中允许存在单文件模块或普通 Python package
- bootstrap 负责将这些目录临时加入 `sys.path`

### 7.4 entry_points 模式（后续阶段）

- group 固定为 `ge.passes.plugins`
- value 可指向模块路径，或返回模块的 callable
- 模块 import 后通过装饰器完成注册

## 8. 三类 pass 的桥接设计

### 8.1 FusionBasePass

最直接的类型，C++ adapter 调用 Python 对象：

- `run(graph, context)`
- 返回值约束为 `None` / `bool` / `int` 三类状态值
- 正式 pass 合同中，`context` 始终为 `PassContext`
- 仅 `_bridge.py` 的 direct bridge/pytest 辅助入口允许传 `None`

该类型优先打通，作为整条链路的最小闭环。

### 8.2 PatternFusionPass

该类型继续复用现有 C++ PatternMatcher 机制。Python 侧只负责：

- 提供 pattern graph
- 根据 `MatchResult` 判断是否满足条件
- 构造 replacement graph

这意味着 C++ 侧需要有一个 Python adapter 继承 `PatternFusionPass`，在以下点回调 Python：

- `Patterns()`
- `MeetRequirements()`
- `Replacement()`

这里有一个明确的方案约束：

- 不要求 Python 用户类直接继承一个通过 `PYBIND11_MODULE` 暴露出来的 C++ `PatternFusionPass`
- 用户继续继承纯 Python 的 `ge.passes.base.PatternFusionPass`
- 复用 C++ 基类公共 `Run()` 流程的职责放在 `PythonPatternFusionPassAdapter` 上，而不是放在 Python 用户类上
- Python 子类禁止覆写 `run()`；若误覆写，基类在类定义阶段直接抛出 `TypeError`，避免“实现了但永远不会被调用”的歧义

推荐形态是：

- `PythonPatternFusionPassAdapter : public PatternFusionPass`
- adapter 覆盖 `Patterns()` / `MeetRequirements()` / `Replacement()`
- 覆盖函数内部再回调 Python pass 实例
- `Run()` 直接复用现有 C++ `PatternFusionPass::Run()`

选择这个方案的原因是：

- 能最大化复用现有 C++ `PatternMatcher`、rewrite、统计和错误处理逻辑
- 不强迫 Python 用户环境必须先成功 import 一个 native C++ 基类模块，保持 `ge.passes` 纯 Python API 的可用性
- 能让 `FusionBasePass`、`PatternFusionPass`、`DecomposePass` 三类 pass 在用户侧保持统一风格
- 能把 Python 版本敏感问题尽量收敛在 adapter / wrapper / native bridge 层，而不是扩散到用户 pass 基类定义层

### 8.3 DecomposePass

该类型继续复用现有 `DecomposePass` 语义。Python 侧只负责：

- `MeetRequirements(const GNode &)`
- `Replacement(const GNode &)`

构造时需要保留 `op_types` 信息。

与 `PatternFusionPass` 一样，Python 用户类不直接接管 `Run()` 主流程，而是只实现 hook：

- `meet_requirements(node) -> bool`
- `replacement(node) -> Graph`

这里的额外合同约束需要明确写死在 Python 基类里：

- Python 子类禁止覆写 `run()`；若误覆写，基类在类定义阶段直接抛出 `TypeError`
- `replacement()` 必须返回 replacement Graph
- 若希望跳过当前节点，必须在 `meet_requirements()` 阶段返回 `False`，不支持通过 `replacement()` 返回 `None`

与 `PatternFusionPass` 相同，这里也不要求 Python 用户类直接继承 pybind 暴露的 C++ `DecomposePass`。推荐形态是：

- `PythonDecomposePassAdapter : public DecomposePass`
- adapter 在 C++ 侧复用 `DecomposePass::Run()`
- adapter 覆盖 `MeetRequirements()` / `Replacement()` 并转调 Python pass 实例

这样可以保留现有 C++ `DecomposePass` 的主流程语义，同时避免把构造参数、`op_types` 和 Python 版本敏感逻辑直接暴露给 Python 基类继承体系。

### 8.4 creator 与上下文获取设计

当前 `CreateFusionPassFn` 是裸函数指针：

- `using CreateFusionPassFn = FusionBasePass *(*)();`

V1 不建议把它直接改成 `std::function<FusionBasePass *()>`。原因有两点：

- Python pass 来自 bridge `.so` 的动态注册，如果 creator 持有可捕获 lambda，析构链容易和 `dlclose` 顺序耦合
- 一旦 `PassRegistry` 或其他全局对象在 bridge `.so` 已卸载后才析构，`std::function` 内部对象析构就可能访问已卸载代码，存在 `coredump` 风险
这里当前不再以“独立 bridge `.so` 的 `dlclose` 风险”作为主原因。更准确的原因是：

- 现有 creator ABI 仍是无参裸函数指针，直接改成可捕获对象会扩大影响面
- `std::function` 会把运行时路由信息、对象析构和调用路径耦合进 creator 本身，不利于维持“creator 只做最小路由、运行时资源放在 bridge/runtime registry”的分层
- 保留裸函数指针 + TLS 路由上下文，仍然是当前影响面最小、最稳妥的方案

因此这里建议采用更稳妥的方案：

- 保留 `CreateFusionPassFn` 为裸函数指针
- 增加一个“创建期 TLS 上下文”
- Python pass 的运行时对象和元数据放在 bridge 持有的进程级注册表中

这里说的“标识信息”不是 Python 运行时上下文本身，而是**用于注册表查找的稳定 key 和元数据**，例如：

- `pass_name`
- `pass_kind`，即 `fusion` / `pattern` / `decompose`
- `stage`
- Python 模块名
- Python 类全名
- `decompose` 场景下的 `op_types`

这些信息属于注册期静态信息，不是执行期上下文。真正的 Python 解释器状态、模块对象、pass 实例，不放在 creator 里携带，而是放在 bridge 的全局注册表中统一管理。

推荐实现方式如下：

1. 在消费 `create_fn()` 的位置设置 TLS 创建上下文
- 最终建议传入 `descriptor_key`
- 某些现有调用点短期更容易拿到 `pass_name` 时，可先做过渡映射，但不建议把 `pass_name` 固化为最终 creator 路由键

2. 为三类 Python pass 提供少量通用 creator 函数
- `CreatePythonFusionPass()`
- `CreatePythonPatternPass()`
- `CreatePythonDecomposePass()`

3. 通用 creator 从 TLS 中读取当前 `descriptor_key`
- 再据此从 bridge 注册表中找到对应 descriptor
- 然后构造对应 adapter

4. adapter 在执行时再从 bridge 注册表获取 Python pass 实例或其 holder

这个设计的优点是：

- 不需要把 Python 对象上下文塞进 `create_fn`
- 不引入 `std::function` 的析构顺序风险
- 与当前 GE 的 creator 调用方式兼容性更好

### 8.5 TLS 创建上下文细化

当前实现需要保留一个轻量的创建期 TLS 上下文。它的必要性不是“为了传更多信息”，而是因为当前 `FusionPassRegistrationData::CreatePassFn` 仍是无参 creator，而 Python pass 侧多个 descriptor 会共用同一个 adapter factory；如果没有这份 TLS，上层 `PassRegistry::CreatePass()` 虽然知道“正在创建哪个 pass”，共享 factory 却不知道应该绑定哪个 Python descriptor。

当前代码形态可以表示为：

```cpp
struct PythonPassCreateContext {
  std::string descriptor_key;
  std::string pass_name;
  PythonPassKind kind;
};
```

并提供如下辅助设施：

- `SetCurrentPythonPassCreateContext(descriptor_key)`
- `GetCurrentPythonPassCreateContext()`
- `ClearCurrentPythonPassCreateContext()`
- RAII scope guard，例如 `PythonPassCreateScope`

使用方式如下：

1. 在调用 `create_fn()` 前，由调用方设置当前 `descriptor_key`
2. 通用 creator 读取 TLS 中的 `descriptor_key`
3. 通用 creator 再到 bridge 注册表中查找对应 descriptor
4. 构造对应 adapter
5. `create_fn()` 返回后自动清理 TLS

其中：

- `descriptor_key` 是真正的路由主键
- `pass_name` / `kind` 当前主要承担一致性校验，避免共享 factory 误绑定到错误 descriptor

后续如果调用链进一步统一，仍建议把 `PythonPassCreateContext` 收敛到“只保留 `descriptor_key` 这一最小必要字段”，避免状态重复，并与运行时 descriptor / runtime entry 的主键模型保持一致。

V1 建议在以下调用点接入该 scope：

- `compiler/graph/fusion/pass/fusion_pass_executor.cc`
- `FusionPassExecutor::InitPassesIfNeed`

其中：

- 首批与后续主链路都只覆盖 `FusionPassExecutor`
- `graph_fusion.cc` 不在本方案后续支持范围内

这样可以避免把 Python 化范围扩散到 legacy 兼容链路，同时保证 creator / TLS / descriptor 方案围绕主链闭环演进。

### 8.6 bridge 进程级注册表细化

bridge 内部建议维护两个层次的注册信息，而不是继续让一个 `holder_key` 同时承担“静态身份”和“运行时实例”两种语义。逻辑上可拆成两部分：

- `PythonPassDescriptor`
- 注册期静态信息

- `PythonPassInstanceHolder`
- 执行期实例信息
- Python pass 实例
- 运行期状态
- 异常状态
- session / instance 关联信息

其中 `PythonPassDescriptor` 建议至少包含：

- `pass_name`
- `pass_kind`
- `stage`
- `module_name`
- `class_qualname`
- `op_types`
- `descriptor_key`

其中：

- `descriptor_key`
- 表示“这个 pass 类是谁”的静态 key
- 建议格式为 `module_name + class_qualname + pass_name`
- 用于注册去重、descriptor 查找、日志定位

- `instance_id`
- 表示“这次运行时实例是谁”的动态 key
- 由 adapter / session 在创建时生成
- 用于 holder 查找、实例生命周期管理、执行期隔离

最小实现阶段曾把 `holder_key` 同时用于 descriptor 查找和 holder 查找。当前 `FusionBasePass` 已完成拆分，后续其余 pass 也应保持这套模型：

- `descriptor_key`
- 静态身份

- `instance_id`
- 动态实例身份

V1 建议注册表只由 bridge 自己持有和析构，不暴露给其他全局单例持有对象，避免再次产生跨 so 生命周期耦合。

这里刻意不把 Python pass 实例做成进程级单例。原因是：

- 用户更容易自然地把 `self` 当成“本次 pass 执行的临时状态容器”
- 可以避免跨图、跨执行残留状态污染
- 可以降低多线程并发下实例共享导致的锁和重入要求

### 8.7 Python pass adapter 细化

V1 建议为三类 pass 分别提供 adapter：

- `PythonFusionBasePassAdapter`
- `PythonPatternFusionPassAdapter`
- `PythonDecomposePassAdapter`

三者共同特征：

- 构造时只接收 `descriptor_key` 或 `pass_name`
- 构造时不直接持有 Python 临时对象裸指针
- 构造期完成 descriptor 绑定，并为当前 adapter 创建独立 `instance_id`
- adapter 生命周期内独占自己的 Python pass 实例，不复用长期共享 holder
- 执行时通过 `instance_id` 在 bridge 实例仓库中查找当前 adapter 对应的 holder
- 析构时通过 `instance_id` 释放 holder
- 执行时统一做 GIL 获取、异常转译、状态映射

这样即使 adapter 本身由 GE 长期持有，它也只依赖 bridge 稳定管理的 holder，不依赖 creator 闭包对象。

三类 adapter 的分工还需要进一步明确：

- `PythonFusionBasePassAdapter`
- 直接覆盖 `Run()`，内部调用 Python `run(graph, context)`

- `PythonPatternFusionPassAdapter`
- 继承 C++ `PatternFusionPass`
- 不重写基类公共 `Run()` 主流程
- 只覆盖 `Patterns()` / `MeetRequirements()` / `Replacement()` 三个 hook，并在 hook 内部转调 Python

- `PythonDecomposePassAdapter`
- 继承 C++ `DecomposePass`
- 不重写基类公共 `Run()` 主流程
- 只覆盖 `MeetRequirements()` / `Replacement()`，并在 hook 内部转调 Python

这也是当前设计里“为什么不急着把 C++ pass 基类通过 `PYBIND11_MODULE` 暴露给 Python 继承”的核心原因：真正需要复用 C++ 非纯虚主流程的是 adapter，不是用户写的 Python pass 类。

### 8.8 执行期 session 设计

为避免对 Python pass 写法施加不必要限制，V1 建议引入“每次执行一个 session”的模型：

- 一个 adapter 的一次 `Run` 调用，对应一个 `PythonPassExecutionSession`
- session 内创建新的 Python pass 实例，并分配唯一 `instance_id`
- 同一 session 内，多次 Python 回调共用同一个实例
- session 结束时统一释放该实例及其临时包装对象

这意味着：

- `FusionBasePass`
- 一次 `Run` 对应一个 Python 实例

- `PatternFusionPass`
- 一次 `Run` 内部的 `Patterns`、`MeetRequirements`、`Replacement` 共用同一个 Python 实例

- `DecomposePass`
- 一次 `Run` 内部对多个匹配节点的处理共用同一个 Python 实例

这样设计后，Python 用户可以自然地使用：

- `self.xxx` 作为一次执行期间的临时缓存
- 普通 Python 对象作为辅助状态
- 普通异常作为失败信号

而无需理解“这个实例是不是跨图复用”这种 bridge 内部细节。

### 8.9 内存管理细化

V1 的内存管理目标是：

- 不要求 Python 用户手工释放任何 bridge 对象
- 不要求 Python 用户显式使用 `with`、`close()`、`release()` 之类接口
- 不允许因为用户把对象存在局部变量或成员变量里就触发双释放或悬挂指针

建议按三层对象分别处理。

#### 8.9.1 注册期对象

注册期对象包括：

- descriptor
- Python 模块对象
- Python 类对象
- descriptor 注册表

这些对象由 bridge 注册表统一持有，桥接层负责引用计数和清理。对 Python 用户透明。

#### 8.9.2 执行期对象

执行期对象包括：

- instance holder
- Python pass 实例
- callback 中创建的 `Graph` / `Node` / `MatchResult` / `NodeIo` Python 包装对象
- 可能的临时 `TensorDesc` / `Shape` / `Tensor` 包装对象
- `instance_id`

这些对象都绑定到 execution session，而不是绑定到全局 descriptor。

session 结束时：

- Python pass 实例释放
- 执行期包装缓存释放
- 执行期有效性 token 失效

#### 8.9.3 borrowed 图对象

`Graph`、`Node`、`Tensor` 等对象很多是对 GE 当前执行图的借用视图。为保证 Python 体验不变差，建议：

- Python 包装对象内部持有一个执行期 owner token
- 在 owner token 有效时，所有访问都正常工作
- 一旦用户把对象跨 session 保存并再次访问，不允许崩溃，而是抛出明确的 Python 异常，例如：
- `RuntimeError: graph handle has expired`

这样做的效果是：

- 不要求文档里给用户增加“不要缓存这些对象”的硬限制
- 即使用户这么写，也应得到可理解的 Python 错误，而不是 coredump

#### 8.9.4 TensorDesc / Shape 的值语义

`TensorDesc`、`Shape` 这类对象建议按值语义暴露给 Python：

- Python 获得的是独立对象
- 可安全保存在局部变量或 `self` 上
- 不依赖原始 borrowed graph 句柄继续存活

这样更符合 Python 用户预期，也能减少悬挂引用问题。

#### 8.9.5 bridge 卸载与析构顺序

GE 提供两级卸载语义，分别对应"一轮业务结束"和"进程退出"两个生命周期：

##### Unload — 业务级卸载

一轮图编译完成后，GE 调用 `UnloadPassPlugins()` 清理本轮 pass 注册态，但不关闭 Python 解释器也不卸载 bridge so。这样下一轮业务可以复用已初始化的 Python 运行时，避免反复初始化/终结带来的开销。

当前实现链路：

```
UnloadPassPlugins()
  → PassPluginLoader::Unload()                              [pass_plugin_loader.cc]
    ├─ UnloadPythonFusionBasePasses()                        // 仅在 python_pass_loaded_ 为 true 时执行
    │   → BridgeLoader::Unload()                             [bridge_loader.cc]
    │     ├─ api_->reset_bridge_state()                      // 通知 bridge 清理 Python 侧状态并释放 bridge 模块引用
    │     ├─ ClearPythonFusionBasePassRuntimeRegistry()      // 清理 C++ 侧 runtime 注册表
    │     └─ PassRegistry::ClearPythonPasses()               // 清理 C++ 侧 pass 注册表
    │   python_pass_loaded_ = false
    └─ CustomPassHelper::Unload()                            // 清理 C++ 自定义 pass
```

Unload 不触及 Python 解释器生命周期和 bridge so 句柄，确保下一轮 `Load()` 可以直接复用。

##### ShutdownForProcess — 进程级关闭

进程退出时，GE 调用 `ShutdownPassPluginsForProcess()` 执行完整的资源释放。当前有 3 个入口可以触发：

- `GEFinalizeV2()` — 在线模式进程结束时
- `aclgrphBuildFinalize()` — 离线编译结束时
- `GeGenerator::Finalize()` — 生成器模式结束时

当前实现链路：

```
ShutdownPassPluginsForProcess()
  → PassPluginLoader::ShutdownForProcess()                   [pass_plugin_loader.cc]
    ├─ 一次性守卫: if (shutdown_done_) return               // 确保进程级 shutdown 只执行一次
    ├─ shutdown_done_ = true
    │
    ├─ if (python_pass_loaded_):
    │   UnloadPythonFusionBasePasses()                       // 先清理注册态（同 Unload）
    │     → BridgeLoader::Unload()
    │       ├─ api_->reset_bridge_state()
    │       ├─ ClearPythonFusionBasePassRuntimeRegistry()
    │       └─ PassRegistry::ClearPythonPasses()
    │   python_pass_loaded_ = false
    │
    ├─ ShutdownPythonFusionBasePassesForProcess()            // 无条件执行
    │   → BridgeLoader::ShutdownForProcess()                 [bridge_loader.cc]
    │     ├─ if (api_ != nullptr):
    │     │   api_->shutdown_bridge()                        // 调用 bridge so 导出的 shutdown
    │     │     → PybindBridge::Shutdown()                   [pybind_bridge.cc]
    │     │       ├─ ResetBridgeStateUnlocked()              // 清理 Python 侧状态、释放 bridge 模块引用并 gc.collect()
    │     │       └─ if (owns_interpreter_):                 // 仅当解释器由 bridge 自己拉起时
    │     │           py::finalize_interpreter()             // 终结 Python 解释器
    │     │     owns_interpreter_ = false
    │     ├─ api_ = nullptr                                  // 置空，防止后续再调用
    │     ├─ if (handle_ != nullptr):
    │     │   dlclose(handle_)                               // 卸载 bridge so
    │     │   handle_ = nullptr                              // 置空，防止 dlclose 重复
    │     └─ loaded_path_.clear()
    │
    └─ CustomPassHelper::Unload()                            // 清理 C++ 自定义 pass
```

##### 幂等性保证

由于 `ShutdownPassPluginsForProcess()` 可能从多个入口被重复调用，整条链路通过以下守卫保证幂等：

1. **PassPluginLoader 层** — `shutdown_done_` 标志：首次执行后置为 `true`，后续调用直接返回 `SUCCESS`
2. **BridgeLoader 层** — `api_` / `handle_` 空指针守卫：首次执行后置为 `nullptr`，后续调用跳过 shutdown 和 dlclose
3. **PybindBridge 层** — `Py_IsInitialized()` 守卫：解释器已终结后不再进入 Python 清理逻辑；`owns_interpreter_` 守卫确保只终结自己初始化的解释器

##### 卸载顺序的核心约束

当前实现遵循以下顺序原则：

1. **先清理 C++ 注册表，再 dlclose bridge so** — `UnloadPythonFusionBasePasses()` 先清理 `PassRegistry` 和 `PythonFusionBasePassRuntimeRegistry`，之后才执行 `ShutdownForProcess()` 进行 `dlclose`。这保证 dlclose 时没有任何 C++ 对象仍在持有 bridge 侧的回调函数指针。
2. **先清理 Python 对象，再终结解释器** — `PybindBridge::Shutdown()` 先调用 `ResetBridgeStateUnlocked()` 清理 Python 侧注册表、holder 和动态加载的 pass 模块，并在 reset 内释放 `bridge_module_` 引用、调用 `gc.collect()` 打破循环引用，最后才调用 `py::finalize_interpreter()`。
3. **先终结解释器，再 dlclose so** — `shutdown_bridge()` 在 `BridgeLoader::ShutdownForProcess()` 中先于 `dlclose(handle_)` 执行，保证 dlclose 时 Python 解释器已不再运行。
4. **如果解释器已被外部终结** — `Py_IsInitialized()` 返回 0，bridge 跳过所有 Python 清理逻辑，仅清理 C++ 侧状态，不会对已释放的 Python 对象做 `DECREF`。

这里优先保证"不崩溃"，而不是极限回收所有尾声内存。CPython 的内部 arena 分配器在 `Py_Finalize()` 后可能仍有残余内存不被回收，这是 CPython 的已知行为，不影响进程正常退出。

### 8.10 锁与 GIL 策略细化

锁和 GIL 的设计目标是：

- 不把锁概念暴露给 Python 用户
- 不要求 Python pass 作者自己理解或管理 GIL
- 在 bridge 内部把锁粒度控制到最小，避免把整个 GE pass 执行路径串行化

建议分三类锁。

#### 8.10.1 bridge 管理锁

用于保护：

- 注册表初始化
- 插件发现
- holder 懒加载
- unload / finalize 状态切换

这类锁只包围 bridge 自己的状态管理，不包围用户 pass 逻辑执行。

#### 8.10.2 execution session 锁

每个 execution session 可有自己的轻量状态保护，但不建议让多个 session 共享粗粒度互斥锁。

目标是允许：

- 不同 pass 的不同执行互不阻塞
- 非 Python 的纯 C++ 匹配/改图逻辑继续按原有路径运行

#### 8.10.3 Python GIL

统一规则如下：

- 进入 Python 前获取 GIL
- 离开 Python 后立即释放 GIL
- 纯 C++ 图匹配、图遍历、数据整理逻辑不持有 GIL

对三类 pass 的具体策略：

- `FusionBasePass`
- 回调 `run` 时持有 GIL
- Python 返回后立刻释放 GIL

- `PatternFusionPass`
- C++ pattern 匹配过程不持有 GIL
- 调 `Patterns`、`MeetRequirements`、`Replacement` 时短时持有 GIL

- `DecomposePass`
- C++ 搜索匹配节点时不持有 GIL
- 调 `meet_requirements`、`replacement` 时短时持有 GIL

这样可以把 Python 执行和 C++ 执行边界清楚分开，减少不必要的全局串行化。

#### 8.10.4 回调重入策略

V1 建议默认支持“多 session 并发、单 session 内串行”：

- 一个 execution session 内部不做并发 Python 回调
- 不同 session 之间如果由 GE 上层并发触发，则通过 GIL 自然串行进入 Python

这对 Python 用户的含义是：

- 不需要为了 bridge 额外给 pass 写锁
- 如果用户自己使用模块级全局可变状态，仍需自行保证逻辑正确

bridge 不额外限制用户这么写，但也不为用户自己的全局共享状态提供自动事务语义。

### 8.11 Pythonic 体验约束

V1 的设计原则是“把生命周期和并发复杂度收敛在 bridge 内部”，尽量不给 Python 用户增加非 Pythonic 规则。具体要求如下：

- 不要求用户手工管理内存
- 不要求用户手工管理锁或 GIL
- 不要求用户通过特定上下文管理器才能写 pass
- 不要求用户为了避免复用问题而人为拆散普通 Python 写法

在可做到的范围内，Python 用户应当能按普通类来写：

- 用构造函数初始化固定配置
- 用 `self` 保存一次执行内的临时状态
- 用普通 Python 异常表示错误
- 用普通返回值表示结果

需要如实说明的边界只有两类：

- 注册协议边界
- 用户仍需通过装饰器或等价注册接口声明 pass，这属于框架接入协议，不属于非 Pythonic 限制

- 过期对象边界
- 如果用户跨执行长期保存 borrowed graph 视图对象，后续再次访问时会得到 Python 异常，而不是被静默支持到无限期

这两类边界是框架接入所必需的，但不应把用户逼到“必须按非 Pythonic 模式写代码”。

### 8.12 `REGISTER_CUSTOM_PASS` 后续支持设计

`REGISTER_CUSTOM_PASS` 需要支持，但建议放在三类 `PassRegistry` pass 稳定之后的扩展阶段实施。原因是：

- 其执行路径与 `FusionPassExecutor` 体系不同，当前更多走 `CustomPassHelper` / legacy custom pass 链路
- 首批优先打通三类 pass，能更快把 descriptor、session、bridge、holder、GIL、异常隔离这些公共底座做稳定
- 等底座稳定后，再接 `REGISTER_CUSTOM_PASS`，可以显著提高代码复用度，避免再做第二套 Python bridge

推荐复用方式如下：

- 继续复用同一套 Python 发现机制
  - `ge.passes.bootstrap`
  - 当前阶段以环境变量为主路径，后续再补 `entry_points`

- 继续复用同一套 Python 注册表与 descriptor 机制
  - 在 `PythonPassKind` 中新增 `legacy_custom`
  - descriptor 新增 legacy custom pass 所需元数据

- 继续复用同一套 pybind bridge
  - 不另起第二套 Python runtime 初始化
  - 不另起第二套 holder / session 管理

- 继续复用“静态 `descriptor_key` + 动态 `instance_id`”模型
  - 避免 legacy custom pass 再次引入共享 Python 实例导致的状态限制

- 在 C++ 侧新增 `PythonLegacyCustomPassAdapter`
  - 适配 `REGISTER_CUSTOM_PASS` 所需的接口与执行入口
  - 仅在最外层接口适配上与三类 pass 区分

换句话说，`REGISTER_CUSTOM_PASS` 的后续支持应该是：

- 复用同一套 `bootstrap`
- 复用同一套 `registry`
- 复用同一套 `pybind bridge`
- 复用同一套 `session / holder / instance_id`
- 仅新增一层 legacy custom path 的 adapter 和少量 descriptor 字段

这能保证后续扩展不需要再造一套平行体系。

### 8.13 PatternFusionPass 桥接协议

本节定义 C++ pybind bridge 与 `_bridge.py` 之间用于 `PatternFusionPass` 的跨语言调用协议。

#### 8.13.1 协议函数

`_bridge.py` 需实现以下三个函数，供 `libge_python_pass_bridge.so` 在 embed 模式下回调：

1. **`get_pass_patterns(instance_id: str) -> list`**
   - 由 C++ 侧 `PythonPatternFusionPassAdapter::Patterns()` 通过 bridge 回调
   - `_bridge.py` 调用 Python pass 实例的 `patterns()` 方法
   - 返回 Pattern 对象列表，每个 Pattern 由 `_ge_pass_native.so` 构造

2. **`call_meet_requirements(instance_id: str, match_result_handle: int) -> bool`**
   - 由 C++ 侧 `PythonPatternFusionPassAdapter::MeetRequirements()` 通过 bridge 回调
   - `match_result_handle` 为 C++ `MatchResult*` 的 `uintptr_t` 表示
   - `_bridge.py` 通过 `_ge_pass_native.so` 将其还原为 borrowed MatchResult wrapper
   - 调用 Python pass 实例的 `meet_requirements()` 方法
   - 返回是否满足条件

3. **`call_replacement(instance_id: str, match_result_handle: int) -> int`**
   - 由 C++ 侧 `PythonPatternFusionPassAdapter::Replacement()` 通过 bridge 回调
   - `match_result_handle` 同上
   - 调用 Python pass 实例的 `replacement()` 方法
   - 要求 Python pass 必须返回 replacement Graph
   - 若当前匹配不应继续，必须在 `meet_requirements()` 阶段返回 `False`
   - `_bridge.py` 负责校验返回值类型，并将 Graph 所有权转交给 C++

#### 8.13.2 所有权与生命周期约定

##### Pattern 所有权转移（get_pass_patterns）

- Python 侧通过 `_ge_pass_native.so` 构造 Pattern 对象
- Pattern 为 `unique_ptr` 语义，函数返回前 Python 侧必须调用 `release()` 释放所有权
- C++ 侧通过 `unique_ptr<Pattern>` 接管裸指针，负责后续析构
- **约束**：Python 侧在函数返回后不得继续持有 Pattern 引用

##### MatchResult 借用语义（call_meet_requirements / call_replacement）

- `MatchResult` 所有权始终在 C++ 侧（由 `PatternFusionPass::Run()` 持有）
- Python 侧获得的 MatchResult 为 borrowed 视图，不拥有所有权
- **约束**：Python 侧不得在回调返回后继续持有或缓存 MatchResult 引用
- native binding 应提供 borrowed wrapper，并在过期访问时抛出 `RuntimeError` 而非崩溃
- `uintptr_t` 传递为过渡方案，待 MatchResult native binding 就绪后替换为类型安全的方式

##### Replacement Graph 所有权转移（call_replacement）

- Python 侧构造 replacement Graph
- Graph 所有权通过 `release()` 转移给 C++ 侧（`GraphUniqPtr` / `unique_ptr<Graph>`）
- **约束**：Python 侧在函数返回后不得继续持有该 Graph 引用

#### 8.13.3 DecomposePass 桥接协议

`DecomposePass` 复用 C++ `DecomposePass::Run()` 的匹配与替换主流程，因此 `_bridge.py` 只需补两段按节点回调的协议函数：

1. **`call_decompose_meet_requirements(instance_id: str, node_handle: int) -> bool`**
   - 由 C++ `PythonDecomposePassAdapter::MeetRequirements()` 通过 bridge 回调
   - `node_handle` 为当前 `GNode*` 的 `uintptr_t` 表示
   - `_bridge.py` 通过 `_ge_pass_native.so` 的 `borrow_node()` helper 把它还原为短生命周期 Python `Node` 视图
   - 调用 Python pass 实例的 `meet_requirements()` 方法

2. **`call_decompose_replacement(instance_id: str, node_handle: int) -> int`**
   - 由 C++ `PythonDecomposePassAdapter::Replacement()` 通过 bridge 回调
   - `_bridge.py` 调用 Python pass 实例的 `replacement()` 方法
   - 要求 Python pass 必须返回 replacement Graph
   - 若当前节点不应继续，必须在 `meet_requirements()` 阶段返回 `False`
   - `_bridge.py` 负责校验返回值类型，并通过 `release_graph()` 把所有权转交给 C++

3. **`_bridge.py` 内部实例分派采用明确基类，而不是 `Any`**
   - holder 中保存的实例类型收敛为 `FusionBasePass`
   - `PatternFusionPass` / `DecomposePass` 协议入口在调用前分别做 `isinstance` 收敛
   - `replacement()` 的 bridge 参数与返回值按正式 `Graph` / `Node` 接口处理，不再以无约束 `Any` 传递

##### Node 借用语义（call_decompose_meet_requirements / call_decompose_replacement）

- `DecomposePass::Run()` 在 C++ 侧枚举匹配节点
- Python 侧看到的 `Node` 是由 `_ge_pass_native.so` 基于当前 `GNode` 构造的短生命周期视图
- Python 侧不应跨回调缓存该 `Node`
- 因为该视图本质上仍指向真实图节点，所以读取名称、类型、属性和张量描述等信息时能保持与当前图一致

建议把 legacy custom pass 的接入拆成两层：

- 发现与注册层
  - 继续复用 `ge.passes.bootstrap`、`ge.passes.registry`、`ge.passes._bridge`
  - Python 侧只需新增 `legacy_custom` descriptor 字段与装饰器/注册入口

- 执行适配层
  - 在现有 `CustomPassHelper` / legacy custom pass 链路上增加 `PythonLegacyCustomPassAdapter`
  - 该 adapter 继续复用同一套 pybind bridge、同一套 `descriptor_key -> instance_id` 生命周期协议

这样做的直接收益是：

- Python pass 的发现、模块加载、异常翻译、GIL、session、holder 回收只维护一套实现
- legacy custom pass 只新增最外层接口适配，而不复制 Python runtime 托管逻辑
- 后续若要同时支持 `PassRegistry` pass 与 `REGISTER_CUSTOM_PASS`，两边仍然共享同一份 Python 用户开发体验

## 9. Python 图接口补齐设计

### 9.1 必补能力

为支持 Python 改写现有 `examples/fusion_pass`，至少补齐：

- `Graph` borrowed/non-owning 句柄模式
- `Node.get_input_desc`
- `Node.get_output_desc`
- `Node.update_input_desc`
- `Node.update_output_desc`
- `Node.get_input_const_tensor`
- `Shape`
- `TensorDesc`
- `GeUtils.InferShape`
- `GeUtils.CheckNodeSupportOnAicore`

### 9.2 borrowed handle

运行时从 C++ 回传给 Python 的 `Graph`、`Node`、`Tensor` 很多都不应由 Python 析构。当前 `Graph._create_from(handle, owns_handle, owner)` 已支持 borrowed / non-owning 形态，bridge 在执行期通过：

- `_create_from(handle, owns_handle=False, owner=...)`

把 runtime `GraphPtr` 映射为正式 `ge.graph.Graph` 视图，并用 `owner` 持有创建期 token，避免 Python wrapper 提前析构后把底层 runtime graph 误释放。

## 10. 打包与发布设计

### 10.1 产物

当前仓负责构建：

- `ge_py` 主 wheel
- 多版本 native 子 wheel
- bridge artifact set 与其装载逻辑

这里的 bridge artifact set 指：

- `libge_python_pass_bridge.so`
- 由 GE 内部 loader `dlopen` 的私有 bridge so
- 负责 embed 路径、解释器管理、GIL、Python 回调、异常翻译

- `_ge_pass_native.so`
- 由 Python import 的 helper extension
- 负责 `Graph` / `PassContext` / `MatchResult` 等 native-backed wrapper 与 helper
- 不承载用户可继承的 pass 基类

### 10.2 版本策略

正式支持矩阵：

- `cp39`
- `cp310`
- `cp311`
- `cp312`
- `cp313`
- `cp314`

这些子 wheel 承载的是 Python pass bridge / adapter 相关的 `pybind11` 扩展，而不是替换现有 `ge.graph` 的全部 Python 封装。

发布链路需要覆盖上述 Python 版本的构建能力，但这不等价于“单机必须同时安装所有 Python 版本”。推荐按多版本构建矩阵组织：

- 每个构建 job/container 只负责一个 Python minor 版本
- 每个 job 产出对应 tag 的 native 子 wheel
- 主 wheel 仍保持单份构建

换句话说，要求的是“整体发布流水线覆盖 `cp39-cp314`”，而不是“每台开发机或构建机必须同时具备全部版本”。

当前实现状态：

- `build_python_pass_native_matrix.py` 内置支持版本集合 `cp39/cp310/cp311/cp312/cp313/cp314`；CMake 不再单独维护一份版本列表，`--tag` 仅用于手工调用指定构建版本
- `ge_python` 正式 target 产出主 wheel，并触发 `ge_python_pass_native_wheel_matrix` 尽力构建可用 Python minor 版本的 native 子 wheel
- `ge_python_native_wheel` 是单版本开发 target，只输出当前 `HI_PYTHON` 对应的 artifact set 和 `ge_py_pass_bridge` native 子 wheel
- 仓内在 `python_pass_native_build` 构建工具目录下提供 `build_python_pass_native_matrix.py` 与 `ge_python_pass_native_wheel_matrix` 目标，用于在 CI/本地环境中自动嗅探可用的 `python3.9` 到 `python3.14`；matrix 不重新配置整仓 CMake，而是复用父构建已生成的 compile/link 元数据，仅替换 Python include/lib 后重编 `libge_python_pass_bridge.so` 与 `_ge_pass_native.so`
- 解释器发现顺序为：显式传入的 `--python`、`PATH` 中的 `python3.9` 到 `python3.14`/`python3`/`python`、Conda 环境中的 `bin/python`；Conda 环境优先通过 `conda env list --json` 获取，失败时再扫描 `CONDA_PREFIX`/`CONDA_EXE` 推导出的 `envs` 目录以及常见 `~/miniconda3/envs`、`~/anaconda3/envs`、`~/.conda/envs`
- matrix 构建依赖父构建先完成当前 `HI_PYTHON` 的 `ge_python_native_wheel`，并复用父构建的编译器、编译选项、include/link 元数据、已构建 GE 依赖库和 `CMAKE_CXX_COMPILER_LAUNCHER`；该流程不再重新进入 `cmake/package.cmake` 或重新计算 `BUILD_COMPONENT`
- 主 wheel 只装配纯 Python 代码，不再内置当前构建 Python 的默认 native artifact set
- native 子 wheel 通过标准 `bdist_wheel` 生成，承载 `ge/passes/python_pass_artifacts/<python_tag>-<platform>` 下的一套 bridge/native artifact set，并额外放置当前 tag 的 `ge/passes/_ge_pass_native.so` 兼容副本，保证默认 legacy bridge 路径仍可通过 `import ge.passes._ge_pass_native` 工作
- ge-compiler run 包中继续安装原有 `ge_py-0.0.1-py3-none-any.whl`，并额外把 matrix 输出目录中的 `ge_py_pass_bridge-*.whl` 安装到 `ge-compiler/lib64`
- 运行时选择优先级为“预制 artifact set > 同目录 legacy bridge”；runtime fallback codegen 作为后续独立阶段接入

run 包安装脚本需要注意：run 包可以携带多个 `ge_py_pass_bridge` native 子 wheel，但单次安装只应安装一个与“执行安装脚本的 Python 解释器”兼容的子 wheel。推荐让 pip 基于 wheel tag 自动选择：

```bash
PYTHON_BIN=${PYTHON_BIN:-python3}
LIB_DIR=<run-package>/ge-compiler/lib64

"${PYTHON_BIN}" -m pip install \
  --no-index \
  --find-links "${LIB_DIR}" \
  "${LIB_DIR}/ge_py-0.0.1-py3-none-any.whl" \
  ge-py-pass-bridge
```

该方式下，`ge_py` 主 wheel 通过文件路径明确安装，`ge-py-pass-bridge` 通过 `--find-links` 在同一目录中选择当前 Python tag 与平台 tag 匹配的 native 子 wheel。例如 Python 3.11 只会选择 `cp311-cp311` wheel。安装脚本不应把 `cp39-cp314` 的所有 native wheel 文件路径一次性传给 pip，否则不兼容当前解释器的 wheel 会被判定为不可安装。

#### 10.2.1 artifact set manifest 与选择机制

Python pass bridge artifact set 采用固定目录布局：

```text
ge/passes/python_pass_artifacts/<python_tag>-<platform>/manifest.json
ge/passes/python_pass_artifacts/<python_tag>-<platform>/libge_python_pass_bridge.so
ge/passes/python_pass_artifacts/<python_tag>-<platform>/_ge_pass_native.so
```

当前 manifest 字段如下：

```json
{
  "python_tag": "cp311",
  "platform": "linux-x86_64",
  "bridge_abi": 1,
  "artifacts": {
    "bridge": "libge_python_pass_bridge.so",
    "native": "_ge_pass_native.so"
  }
}
```

字段含义：

| 字段 | 含义 | 匹配方式 |
|------|------|----------|
| `python_tag` | artifact 绑定的 CPython minor version tag，例如 `cp39`、`cp310`、`cp311`、`cp312` | 必须与当前进程内目标 Python runtime key 一致 |
| `platform` | artifact 绑定的平台 tag，当前由系统名和机器架构组成，例如 `linux-x86_64` | 必须与当前运行平台一致 |
| `bridge_abi` | `ge_compiler.so` 内 loader 与 `libge_python_pass_bridge.so` 之间的 C ABI 协议版本 | 必须与 `kPythonFusionPassBridgeAbiVersion` 一致 |
| `artifacts.bridge` | 相对 manifest 所在目录的 bridge `.so` 路径 | 必须能解析到真实文件，作为 `dlopen` 目标 |
| `artifacts.native` | 相对 manifest 所在目录的 `_ge_pass_native.so` 路径 | 必须能解析到真实文件，并通过 `set_artifact_config` 传给 bridge，保证 Python import 使用同源 native |


选择流程：

1. loader 先解析当前进程目标 Python runtime key。若当前进程已加载 CPython C API 符号，则通过 `Py_GetVersion()` 得到 `cpXY`；若进程内尚无可见 CPython 符号，则按内部原生入口场景探测 `PATH` 中的 `python3`、`python`。
2. loader 根据自身 `.so` 路径推导 `ge` 包目录，并扫描 `ge/passes/python_pass_artifacts` 下的 manifest。
3. manifest 先做 JSON 结构和 artifact 文件存在性校验，再按 `python_tag`、`platform`、`bridge_abi` 过滤。
4. 命中的 artifact set 优先进入候选列表；若没有命中，再回退到当前同目录默认 `libge_python_pass_bridge.so` 以及系统动态库搜索路径中的同名 `.so`。
5. bridge `dlopen` 成功后，loader 再读取当前进程实际 Python runtime key，与加载前的目标 key 做一致性校验，避免同一进程中误拉起另一套 CPython minor version。

`kPythonFusionPassBridgeAbiVersion` 只描述 loader 与 bridge C API 的协议版本，例如函数表字段、函数语义或调用时序出现不兼容变化时才需要滚动。项目未正式发布 Python pass bridge 前，完整功能开发期间该值保持 `1`，不因内部字段补齐或阶段性开发提交频繁变更。

### 10.3 安装策略

V1 不做安装时 codegen，也不规划单独的 `prepare` 预编译命令。原因包括：

- 标准 wheel 缺少稳定、通用的 post-install 编译机制
- 安装时的 Python 环境不一定就是最终运行时环境
- 安装机不一定具备编译器和开发头文件
- 一旦安装期自动编译失败，可能把纯 Python 部分的可用性也一起拖垮

因此建议采用两层策略：

1. 主流版本走预编译 native 子 wheel
2. 非覆盖版本走运行时 fallback codegen 兜底

也就是说，安装阶段只负责把主 wheel 和可用的 native 子 wheel 安装到位；若当前 Python 版本没有命中预编译产物，再由运行时按需触发 fallback。

对于 run 包中的 packaged default 路径，还需要满足一个固定约束：

- `libge_python_pass_bridge.so` 随 `ge-compiler` 组件一起安装
- 安装目录与 `ge_compiler.so` 保持一致，落到 `ge-compiler/lib64`
- GE 内部 loader 优先按 `ge_compiler.so` 同目录解析该 bridge `.so`
- 后续 `_ge_pass_native.so` 进入 run 包时，也应遵循同一套 artifact set 装配约束

### 10.4 fallback

当当前 Python 版本没有命中预编译子 wheel 时：

- 主 wheel 直接进入 runtime fallback 流程
- 在本地缓存目录生成并编译对应版本的 bridge artifact set
- 成功则启用 Python pass
- 失败则禁用 Python pass，但不影响原有 C++ pass 流程

这里需要特别约束 fallback 的边界：

- fallback codegen 不生成用户 pass 代码
- fallback codegen 不改写 `ge_compiler.so`
- fallback codegen 的目标是生成可替换的 bridge artifact set，而不是局部 patch
- fallback 产物需要同时覆盖 `libge_python_pass_bridge.so` 与 `_ge_pass_native.so`
- fallback 产物承载完整的 Python 版本敏感 bridge 逻辑，并复用与 `ge_compiler.so` 约定好的稳定协议

### 10.5 本地验证约束

当前开发环境中 Python 为 `3.13`，而正式矩阵为 `cp39-cp314`。因此本地验证只能覆盖当前环境可用的 Python tag；完整矩阵需要 CI 或发布流水线在具备对应 Python 版本的环境中分别构建。

### 10.6 pybind 模块边界

V1 建议把 pybind 侧内容控制在“pass bridge 必需能力”范围，不扩大到整个 `ge.graph`。建议拆成以下边界：

- 主 wheel 中的纯 Python 代码
- `ge.passes.base`
- `ge.passes.registry`
- `ge.passes.bootstrap`
- `ge.passes.runtime`
- `ge.passes._bridge`

- `ge_compiler.so` 内部稳定核心
- `PassPluginLoader`
- `PassRegistry` 注册与运行期路由
- `descriptor_key + instance_id` 生命周期协议
- adapter 回调、异常翻译、错误码映射
- 与独立内部 bridge `.so` 交互的稳定接口

- 预编译 / fallback 生成的独立内部 bridge `.so`
- Python 版本敏感的 `Python.h` / `pybind11` 绑定代码
- Python 解释器初始化、GIL 管理、模块导入、异常转译
- 调用 `ge.passes._bridge` 的统一运行路径

- 预编译 / fallback 生成的 `_ge_pass_native.so`
- `Graph` / `PassContext` / `MatchResult` / `NodeIo` 等正式 Python wrapper 的构造与转换
- Python 侧直接 import 的 helper 与工厂接口
- 对 `ge.passes.base` / `ge.passes.pattern` 提供正式的 native-backed 对象来源

换句话说：

- `libge_python_pass_bridge.so` 的职责是“支撑 Python pass 接入 GE”
- `_ge_pass_native.so` 的职责是“为 Python pass 暴露 native-backed 对象和 helper”
- 二者都不是“重写现有 Python 图 API”

不同 Python 版本之间的差异，原则上不体现在维护多份不同语义的 `.cc` 文件，而主要体现在：

- 使用同一套模板或同一套源码
- 编译时的 Python include / libpython / extension suffix / rpath 等构建参数不同
- 个别兼容宏、条件编译或生成元数据不同

也就是说，后续不建议按 `cp39/cp310/cp311/cp312/cp313/cp314` 长期维护多份手写 bridge 源码分支。

### 10.7 pybind 子 wheel 组织建议

建议采用“主 wheel + 内部 bridge native wheel”的组织方式：

- 主 wheel 包名
- 保持 `ge_py`

- native bridge wheel
- 以单独包名承载，例如逻辑上可命名为 `ge_py_pass_bridge`
- wheel tag 对应 `cp39-cp314`
- 采用标准 `bdist_wheel` 生成，避免手工拼装 wheel metadata / RECORD / tag

主 wheel 中的 `ge.passes.runtime` 负责：

1. 识别当前 Python 版本
2. 解析并装载匹配的 bridge 产物元数据
3. 若未命中预编译模块，则进入 fallback codegen
4. 将最终 bridge 产物路径与入口信息提供给 GE 内部 loader
5. GE 内部 loader `dlopen` `libge_python_pass_bridge.so` 并建立回调入口
6. `libge_python_pass_bridge.so` 再从同目录加载 `_ge_pass_native.so`

这样主 wheel 逻辑固定，native wheel 只负责不同 Python 版本下的 bridge 实现。

这里要进一步明确一条兼容原则：

- 运行时最终选中的 bridge 产物，应以“预编译产物 > fallback 产物”的优先级覆盖
- 不能要求在 `ge_compiler.so` 链接期就决定要用哪一份生成产物
- 更合理的方式是先由 Python 侧或配置侧解析出目标 bridge 产物，再由 GE 内部 loader `dlopen`
- 因此生成产物需要通过稳定模块名或稳定 C ABI 被动态加载，而不是让 run 包内置 `.so` 成为唯一硬链接目标

从扩展性角度看，bridge 不应继续编入 `ge_compiler.so`。当前若存在这类实现，只能视为临时 bring-up 偏差，而不是正式架构。

### 10.8 fallback codegen 边界

fallback codegen 建议直接生成整套 bridge artifact set，而不是只生成某个局部 wrapper helper。输入固定为：

- 当前 Python 版本
- `libge_python_pass_bridge.so` / `_ge_pass_native.so` 的统一源码或模板
- 当前仓导出的稳定头文件
- 当前 Python 环境对应的 include / libpython / extension suffix 等构建参数
- 当前 run 包暴露的稳定链接依赖

输出固定为：

- 本地缓存目录中的 `libge_python_pass_bridge.so`
- 本地缓存目录中的 `_ge_pass_native.so`
- 对应 manifest / 元数据文件

缓存 key 建议至少包含：

- Python major.minor
- bridge 模板源码 hash
- 编译参数 hash
- GE 版本标识

这样可避免模板更新后误命中旧缓存。

推荐的链接与装载关系如下：

1. `ge_compiler.so` 不在链接期依赖某个 fallback 生成物
2. 运行时解析出最终要使用的 bridge 产物路径
3. GE 内部 loader `dlopen` 该 bridge `.so`
4. bridge `.so` 再通过稳定 ABI 与 `ge_compiler.so` 交互

这样才能保证“生成出来的 `.so` 优先生效”，而不是永远被 run 包里预置的版本覆盖。

### 10.9 当前工程与后续 codegen 的兼容策略

当前工程已经把 `python_fusion_base_pass_pybind_bridge.cc` 从 `ge_compiler.so` 中拆出，第一批改为由 `python_fusion_base_pass_bridge_loader.cc` 负责运行时装载 `libge_python_pass_bridge.so`。为了兼容后续 codegen 演进，后续实现仍需持续遵守以下边界：

- 不把 Python 版本敏感逻辑固化进 run 包自带的 `ge_compiler.so`
- `ge_compiler.so` 只保留稳定 loader、注册表、adapter 协议和最小交互面
- 独立 bridge `.so` 承担完整的 Python 敏感逻辑，并成为预编译 / fallback 的统一替换目标
- `descriptor_key + instance_id`、adapter 回调协议、错误码翻译、holder 生命周期等核心语义保持在 compiler 侧稳定

换句话说，若目标是产品化支持后续扩展，那么独立 bridge `.so` 不应作为“后面再挪”的优化项，而应作为一开始的正式架构边界。

### 10.10 Python 解释器来源与 fallback 选择约束

Python pass bridge 不能假设“进程里只有一种 Python 拉起方式”。后续做多版本预编译与 fallback codegen 时，必须把解释器来源、artifact 版本选择和 finalize 顺序作为同一个生命周期问题处理。

当前需要覆盖以下典型场景：

| 场景 | 当前进程 | Python 解释器状态 | bridge 行为 | fallback / codegen 约束 |
|------|----------|------------------|-------------|--------------------------|
| 外部 Python launcher，例如当前实际的 `python -m ge.pyatc` | bridge 所在进程仍是 `atc.bin` | 外层 Python 只负责拉起 `atc.bin`，不等价于 `atc.bin` 内已有解释器；实际日志显示 TBE 先在 `atc.bin` 内初始化 Python | bridge 在 `atc.bin` 内看到 `Py_IsInitialized()` 为 true 后复用 TBE 解释器，`owns_interpreter=false` | artifact 选择必须以 `atc.bin` 当前进程内解释器版本为准，不能把 launcher Python 或 TBE worker Python 直接当作 bridge 版本依据 |
| Python 进程内通过 `ctypes` / C API 直接调用 GE/ATC 入口 | 当前 Python 主进程 | 进入 GE 前解释器已经初始化 | TBE 与 bridge 都应看到 `Py_IsInitialized()==1`，并复用当前解释器，均不拥有解释器 | artifact 选择必须以当前 Python 进程的 `sys.version_info` 为唯一权威版本；`PATH` 中的 `python3` 不参与该场景的版本选择 |
| 内部原生入口，例如 `atc.bin`，且 Python pass 最先用 Python | `atc.bin` | bridge 进入前解释器未初始化 | bridge 可调用 `py::initialize_interpreter()`，并记录 `owns_interpreter=true` | artifact 选择必须在初始化前基于明确的目标 Python 环境完成，例如配置的 Python executable 或统一 runtime selector；不能隐式随机使用不同来源的 `python3` |
| `atc.bin` 中 TBE 先初始化 Python | `atc.bin` | TBE 的 `TbeInitialize -> PythonAdapterManager::Initialize -> py_decouple.cc` 已初始化解释器 | bridge 必须复用现有解释器，`owns_interpreter=false` | 预编译或 fallback artifact 必须匹配 TBE 已经拉起的进程内 Python minor version；若已选 artifact 与进程内版本不一致，必须报错禁用 Python pass，不能再拉第二个解释器 |
| TBE 并行编译 worker，例如日志中的 `TBE(pid,python3)` | 独立 `python3` 子进程 | 只影响 worker 自己的地址空间 | 与 `atc.bin` 内的 bridge 无共享解释器关系 | 不能把 worker 进程的 Python 版本作为 bridge artifact 选择依据；bridge 只看当前进程 |

#### 10.10.1 外部 Python launcher 实际流程

当前 `python -m ge.pyatc` 这类外部拉起场景，如果实现方式是 `subprocess` / `execve` 启动原生 `atc.bin`，应按“Python launcher 启动原生 `atc.bin`”理解，而不是“GE 直接运行在 launcher Python 进程里”。因此对 Python pass bridge 有效的当前进程仍然是 `atc.bin`。

需要明确的是，`subprocess.Popen(["atc.bin", ...])` 经过 `execve` 后，`atc.bin` 子进程不会继承父 Python 进程内的解释器状态、GIL、`sys.modules`、Python object 或已加载的 libpython 句柄；它只继承环境变量、当前工作目录、部分文件描述符等进程属性。因此外层 launcher Python 版本只能通过 `PATH`、`LD_LIBRARY_PATH`、`PYTHONPATH`、`ASCEND_GE_PY_PASS_PATH` 等环境变量间接影响 `atc.bin`，不能直接作为 bridge artifact 版本依据。

实际日志对应的流程是 TBE 先拉起 Python：

```text
用户 shell
  -> python -m ge.pyatc
      -> 外层 Python launcher 准备参数 / 环境
      -> 启动 atc.bin

atc.bin 进程
  -> TbeInitialize()
      -> PythonAdapterManager::Initialize()
          -> HandleManager::Initialize()
              -> dlsym(RTLD_DEFAULT, "Py_Initialize")
                  -> 若能找到符号:
                       -> LoadFuncs(RTLD_DEFAULT)
                       -> 若 Py_IsInitialized() == 1:
                            -> 复用已有解释器
                       -> 若 Py_IsInitialized() == 0:
                            -> TE_Py_Initialize()
                  -> 若找不到符号:
                       -> LaunchDynamicLib()
                       -> 按 PATH 中的 python3 / python 推导 libpython
                       -> dlopen(libpythonX.Y.so.1.0, RTLD_GLOBAL)
                       -> TE_Py_Initialize()
          -> import te_fusion.* / parallel_compilation
          -> init_multi_process_env()
          -> 拉起独立 python3 worker 子进程

atc.bin 进程
  -> LoadPassPlugins()
      -> dlopen(libge_python_pass_bridge.so)
      -> bridge EnsureBridgeReady()
          -> Py_IsInitialized() == 1
          -> 复用 TBE 已初始化的解释器
          -> owns_interpreter = false
          -> import ge.passes._ge_pass_native / ge.passes._bridge
          -> 注册 Python pass descriptor
```

这个场景下，TBE 是解释器 owner，Python pass bridge 是解释器使用方。外层 launcher Python 与 TBE worker Python 都不是 bridge 所在的解释器。

#### 10.10.2 Python 进程内 `ctypes` / C API 直接调用场景

如果后续存在不启子进程、而是在 Python 主进程内通过 `ctypes.CDLL(...)`、C API wrapper 或类似方式直接调用 GE/ATC 入口函数的模式，GE、TBE 和 Python pass bridge 都运行在同一个 Python 进程地址空间内：

```text
python 主进程
  -> import ge.pyatc / ctypes.CDLL(...)
  -> 调用 main_impl / GE build 入口

同一 python 进程
  -> TbeInitialize()
      -> dlsym(RTLD_DEFAULT, "Py_Initialize")
          -> 应能找到当前 CPython 符号
      -> Py_IsInitialized() == 1
      -> TBE 复用当前 Python 解释器
      -> pyEnvStatusBeforeTbe = true
      -> TBE 不拥有解释器，不应 Py_Finalize

同一 python 进程
  -> LoadPassPlugins()
      -> dlopen(libge_python_pass_bridge.so)
      -> bridge EnsureBridgeReady()
          -> Py_IsInitialized() == 1
          -> 复用当前 Python 解释器
          -> owns_interpreter = false
          -> import ge.passes._ge_pass_native / ge.passes._bridge
          -> 注册 Python pass descriptor
```

这个场景下，解释器 owner 是外层 Python 主进程。TBE 与 Python pass bridge 都只能作为解释器使用方，不能调用 `Py_Finalize()` / `py::finalize_interpreter()`。

该模式下的版本选择最直接，核心原则是复用当前进程已有解释器：

- 当前 Python 进程的 `sys.version_info` 是唯一权威版本。
- `libge_python_pass_bridge.so` 与 `_ge_pass_native.so` 必须匹配该 Python minor version。
- TBE 的 `py_decouple.cc` 在已能通过 `RTLD_DEFAULT` 找到 CPython 符号、且 `Py_IsInitialized()==1` 时，必须复用当前解释器；`PATH` / `python3-config` 分支只应在当前进程没有可见 CPython 符号时进入。
- 如果发现当前进程已初始化的 Python 版本与 bridge artifact manifest 不一致，必须报错禁用 Python pass；同一进程内并存两套 CPython minor version 不属于允许的 fallback 方案。

#### 10.10.3 内部 `atc.bin` 场景：TBE 先拉起

`atc.bin` 原生进程内，如果 TBE 先使用 Python，则 TBE 的 `py_decouple.cc` 负责寻找和初始化 libpython：

```text
atc.bin
  -> TbeInitialize()
      -> dlsym(RTLD_DEFAULT, "Py_Initialize")
          -> 已找到符号:
               -> 说明当前进程已可见 CPython C API
               -> 再调用 Py_IsInitialized()
                    -> true: 复用已有解释器
                    -> false: TBE 调 TE_Py_Initialize()
          -> 未找到符号:
               -> popen("python3 -V")
               -> 失败则 popen("python -V")
               -> 解析 Python major.minor
               -> 拼 so 名:
                    Python 3.7  -> libpython3.7m.so.1.0
                    Python 3.8+ -> libpython3.X.so.1.0
               -> popen("python3-config --prefix")
               -> 拼 $prefix/lib/libpythonX.Y[ m ].so.1.0
               -> mmDlopen(绝对路径, RTLD_NOW | RTLD_GLOBAL)
               -> 若失败:
                    -> mmDlopen("libpythonX.Y[ m ].so.1.0", RTLD_NOW | RTLD_GLOBAL)
                    -> 依赖 LD_LIBRARY_PATH / RUNPATH / ld.so.cache / 系统 lib 路径
               -> TE_Py_Initialize()

atc.bin
  -> LoadPassPlugins()
      -> dlopen(libge_python_pass_bridge.so)
      -> Py_IsInitialized() == 1
      -> bridge 复用 TBE 解释器
      -> owns_interpreter = false
```

这个场景下，fallback / codegen 必须匹配 TBE 已经初始化的 `atc.bin` 进程内 Python minor version。如果 bridge artifact 与该版本不一致，必须禁用 Python pass 或报清晰错误，不能再尝试加载另一套 CPython。

#### 10.10.4 内部 `atc.bin` 场景：Python pass 先拉起

如果某条流程中 Python pass bridge 早于 TBE 使用 Python，则解释器 owner 会变成 bridge：

```text
atc.bin
  -> LoadPassPlugins()
      -> 解析 bridge artifact set
      -> dlopen(libge_python_pass_bridge.so)
          -> libpython 由 bridge 的 ELF NEEDED / RUNPATH / LD_LIBRARY_PATH / ld.so.cache 解析
      -> bridge EnsureBridgeReady()
          -> Py_IsInitialized() == 0
          -> py::initialize_interpreter()
          -> owns_interpreter = true
          -> import ge.passes._ge_pass_native / ge.passes._bridge
          -> 注册 Python pass descriptor

atc.bin
  -> TbeInitialize()
      -> dlsym(RTLD_DEFAULT, "Py_Initialize")
          -> 能找到符号
      -> Py_IsInitialized() == 1
      -> TBE 复用 bridge 已初始化的解释器
      -> pyEnvStatusBeforeTbe = true
      -> TBE 不拥有解释器，不应 Py_Finalize
```

这个场景下要特别小心 shutdown：bridge 虽然 `owns_interpreter=true`，但如果 TBE 后续复用了该解释器，bridge 不能在 TBE 清理自己的 Python module / parallel compilation 前提前 `py::finalize_interpreter()`。因此后续设计必须把“bridge 清理 Python pass 状态”和“解释器最终 finalize”拆成两个动作，或引入统一的进程级 Python runtime manager。

这里有几个必须写死的判断原则：

- `Py_IsInitialized()` 只描述**当前进程**中的 CPython interpreter 状态，不描述外部 `python3` 子进程状态。
- `Py_IsInitialized()` 返回 true 只能说明当前进程已有解释器，不能单独说明应该选择哪个 wheel；版本仍需通过进程内 `sys.version_info` 或 `Py_GetVersion()` 明确读取。
- 能通过 `dlsym(RTLD_DEFAULT, "Py_IsInitialized")` 或 `Py_Initialize` 找到符号，只说明当前进程可见 CPython C API 符号，不等价于解释器已经初始化。
- 同一进程只允许存在一套 CPython minor version；任何模块只要发现当前进程已有解释器，后续 Python pass artifact 选择都必须以该解释器版本为准，不能再根据 `PATH` 中另一个 `python3` 重新选择不同 minor version 的产物。
- `libge_python_pass_bridge.so` 与 `_ge_pass_native.so` 必须匹配同一个 CPython minor version，并作为同一 manifest 描述的 artifact set 一起启用。
- 若 bridge 自己初始化解释器，bridge 才能在进程级 shutdown 时 `py::finalize_interpreter()`；若解释器来自用户 Python 主进程或 TBE，bridge 只能清理自己持有的 module、holder 和 registry 状态，不能 finalize 解释器。

因此，fallback runtime selector 的推荐策略是：

1. 若当前进程内解释器已经初始化，以进程内 Python 版本作为唯一权威版本。
2. 若当前进程内解释器尚未初始化，以显式配置的目标 Python executable 或统一 selector 结果作为目标版本，并用该版本生成 / 选择 artifact set。
3. bridge 初始化后再次读取进程内 Python 版本，与 artifact manifest 做一致性校验。
4. 发现版本不一致时，优先清晰报错并禁用 Python pass；同一进程混用两个 CPython minor version 不属于可接受的恢复路径。

这里特别需要防止一个 shutdown 顺序问题：

```text
TBE 初始化 Python
  -> Python pass bridge 复用解释器并缓存 py::object / holder
  -> TBE 先 Py_Finalize
  -> Python pass bridge 后续继续访问 py::object
```

上述顺序是非法的。更准确的原则不是“bridge 永远先 finalize”或“TBE 永远先 finalize”，而是：

- 所有模块持有的 Python object、module、holder 必须在解释器 finalize 前清理完。
- 真正调用 `Py_Finalize()` / `py::finalize_interpreter()` 的模块，必须是解释器 owner，且必须等同进程内其他 Python 使用方都完成清理后才能执行。
- bridge 的“清理 Python pass 状态”和“finalize interpreter”必须在设计上可分离，不能把二者永久绑成一个动作。

因此不同拉起顺序下的 shutdown 约束不同：

```text
TBE 先初始化 Python:
ShutdownPassPluginsForProcess()
  -> reset / clear Python pass holder、module、registry
GELib::Finalize()
  -> TBE / op store finalize
  -> TBE 清理自己的 Python module / parallel compilation
  -> TBE 在自己拥有解释器时 Py_Finalize

Python pass bridge 先初始化 Python:
ShutdownPassPluginsForProcess()
  -> reset / clear Python pass holder、module、registry
  -> 不能立即 py::finalize_interpreter，除非确认 TBE 等其他 Python 用户尚未初始化
GELib::Finalize()
  -> TBE / op store finalize
  -> TBE 只清理自己的 Python module，不 Py_Finalize
最后由 bridge owner 或进程级 Python runtime manager 再 finalize interpreter
```

后续若调整 `GEFinalize`、`GeGenerator::Finalize`、`aclgrphBuildFinalize`、TBE plugin manager 或 pass plugin loader 的顺序，必须重新检查该约束。只要 bridge 不拥有解释器，就不能假设自己能控制解释器生命周期；只要 bridge 拥有解释器但 TBE 已经复用过该解释器，就不能在 TBE 清理自身 Python module 前先 finalize 解释器。长期更稳妥的方向是引入进程级 Python runtime manager 或引用计数式 owner 协议，把“解释器初始化/复用/最终 finalize”从单个 bridge 或 TBE 模块中抽象出来。

## 11. ATC 扩展设计

为后续 Python ATC 入口做统一预留，当前建议分成两层：

- 当前主路径
- `ASCEND_GE_PY_PASS_PATH`

- 后续产品化入口
- CLI / 内部 option 可以再补显式参数，但最终都应归一到 `ge.passes.bootstrap.load_pass_plugins()` 的统一发现协议

- 与 native companion 模块相关的选择逻辑
- 不建议让用户直接指定某个 pybind bridge 源码或生成脚本
- 更适合由 `ge.passes.runtime` 统一决定“预编译 / fallback”产物选择

后续 Python ATC 入口不应再设计第二套发现机制，而是直接复用：

- `ge.passes.bootstrap.load_pass_plugins()`
- `ge.passes.bootstrap.get_registered_passes()`

## 12. 文件级开发计划

### 12.1 Python 包与发现层

修改和新增如下文件：

- `api/python/ge/setup.py`
- `api/python/ge/ge/__init__.py`
- `api/python/ge/ge/passes/__init__.py`
- `api/python/ge/ge/passes/base.py`
- `api/python/ge/ge/passes/registry.py`
- `api/python/ge/ge/passes/pattern.py`
- `api/python/ge/ge/passes/replacement.py`
- `api/python/ge/ge/passes/bootstrap.py`
- `api/python/ge/ge/passes/_bridge.py`

职责如下：

- 定义三类 pass 的 Python 基类
- 提供装饰器和注册表
- 实现环境变量主路径发现逻辑，并为后续 `entry_points` 预留扩展
- 统一 bridge 对外接口
- 承载 wheel 选择与 fallback 管理

其中 `base.py` 的正式方向需要明确分两层：

- 用户继承层
- 继续暴露纯 Python 的 `FusionBasePass` / `PatternFusionPass` / `DecomposePass`
- 这里不要求用户继承 native C++ 基类

- wrapper 来源层
- `PassContext` / `MatchResult` / `PatternMatcherConfig` 等对象直接从 `_ge_pass_native` 取得 native-backed 实现
- import `_ge_pass_native` 失败属于环境或产物装配问题，不再作为 Python API 的正式 fallback 路线处理

### 12.2 Python graph wrapper 补齐

修改和新增如下文件：

- `api/python/ge/ge/graph/__init__.py`
- `api/python/ge/ge/graph/graph.py`
- `api/python/ge/ge/graph/node.py`
- `api/python/ge/ge/graph/tensor.py`
- `api/python/ge/ge/graph/tensor_desc.py`
- `api/python/ge/ge/_capi/pygraph_wrapper.py`

职责如下：

- 补齐 borrowed handle
- 暴露 tensor desc/shape
- 增补 node 的输入输出 desc 能力
- 补常量 tensor 读取接口

### 12.3 C wrapper 与 native bridge

修改和新增如下文件：

- `api/python/ge/ge_api_c_wrapper/c_graph.cc`
- `api/python/ge/ge_api_c_wrapper/c_gnode.cc`
- `api/python/ge/ge_api_c_wrapper/c_tensor.cc`
- `api/python/ge/ge_api_c_wrapper/c_match_result.cc`
- `api/python/ge/ge_api_c_wrapper/ge_api_c_wrapper_utils.h`
- `compiler/graph/fusion/pass/pass_plugin_loader.cc`
- `compiler/graph/fusion/pass/pass_plugin_loader.h`
- `compiler/graph/fusion/pass/python_fusion_base_pass_bridge_c_api.h`
- `compiler/graph/fusion/pass/python_fusion_base_pass_bridge_loader.cc`
- `compiler/graph/fusion/pass/python_fusion_base_pass_pybind_bridge.cc`
- `compiler/graph/fusion/pass/python_fusion_base_pass_pybind_bridge.h`
- 新增 `_ge_pass_native.so` 的源码、导出头和构建脚本
- `api/python/ge/ge_api_c_wrapper/CMakeLists.txt`
- `api/python/ge/CMakeLists.txt`
- `compiler/CMakeLists.txt`

职责如下：

- 为 Python graph wrapper 提供 C 接口
- 提供基于 `pybind11` 的 Python pass bridge / helper so
- 接入 wheel 打包与安装

其中建议进一步拆分责任：

- `c_graph.cc` / `c_gnode.cc` / `c_tensor.cc` / `c_match_result.cc`
- 继续服务 `ge.graph` / `ge.es` 的 ctypes 路线

- 独立 bridge `.so`
- 负责 Python 运行时初始化、descriptor 同步、holder 管理、adapter 原生逻辑以及 Python/C++ 对象转换
- 负责承接预编译 / fallback 产物

- `_ge_pass_native.so`
- 负责 `Graph` / `PassContext` / `MatchResult` 等 native-backed wrapper 与 helper
- 负责与 `base.py` 的对象来源对接
- 不承载用户 pass 基类，也不要求用户直接 import C++ pass 基类继承

- `pass_plugin_loader.cc/.h`
- 负责定位并 `dlopen` bridge `.so`
- 负责和 bridge `.so` 的稳定 ABI 对接

- `python_fusion_base_pass_bridge_c_api.h`
- 定义 bridge loader 与 `libge_python_pass_bridge.so` 之间的稳定 C ABI
- 当前入口为 `GeGetPythonFusionBasePassBridgeApi()`

- `python_fusion_base_pass_bridge_loader.cc`
- 位于 `ge_compiler.so` 一侧
- 负责 `dladdr` 定位、`dlopen/dlsym`、缓存 bridge API、以及把 registrar 回调传入 bridge
- 当前显式使用 `RTLD_GLOBAL` 装载 bridge，以便 embedded CPython 后续导入标准库/native extension 时能解析到 `libpython` 符号

- `python_fusion_base_pass_pybind_bridge.cc/.h`
- 位于 `libge_python_pass_bridge.so` 一侧
- 负责 Python 运行时初始化、descriptor 同步、holder 生命周期和 `create/run/destroy` 回调实现
- 对 `pass_plugin_loader` 暴露稳定 ABI，对 Python 侧复用 `bootstrap / _bridge` 协议

正式架构边界应是“bridge artifact set 可替换，`ge_compiler.so` 稳定”。其中：

- `libge_python_pass_bridge.so` 是 GE 内部 loader 视角的主入口
- `_ge_pass_native.so` 是 Python 视角的 helper extension
- 二者必须作为同一版本、同一构建 key 的配套产物管理
- 当前第一批已把 `python_fusion_base_pass_pybind_bridge.cc` 从 `ge_compiler` target 中迁出，并新增 `ge_python_pass_bridge` target 产出 `libge_python_pass_bridge.so`
- `ge_compiler.so` 当前只保留 loader、adapter、registry/runtime entry 等稳定语义；bridge so 才直接依赖 `Python3::Python` 与 `pybind_options`

### 12.4 Pass 注册核心改造

修改如下文件：

- `compiler/graph/fusion/pass/pass_registry.cc`
- `compiler/graph/fusion/pass/fusion_pass_executor.cc`
- 新增创建期上下文管理文件，例如：
- `compiler/graph/fusion/pass/pass_create_context.h`
- `compiler/graph/fusion/pass/pass_create_context.cc`

职责如下：

- 在 `create_fn()` 调用点注入 TLS 创建上下文
- 让通用 creator 能按 `pass_name` 找到对应 Python descriptor
- 保持现有 C++ pass 行为不变

建议职责再细化为：

- `pass_create_context.h/.cc`
- 定义 TLS 上下文与 RAII scope

- `fusion_pass_executor.cc`
- 在 `InitPassesIfNeed` 中围绕 `create_fn()` 加 scope

- bridge 注册函数
- 将 Python pass descriptor 注册为“固定 creator 函数 + pass_name 元数据”

说明：

- `graph_fusion.cc` 属于 legacy 兼容链路，不纳入本方案后续支持范围
- `REGISTER_CUSTOM_PASS` 的后续支持走独立扩展阶段，但仍复用同一套 descriptor / bridge / session 机制

### 12.5 A/B 分工与联调边界

当前建议按以下边界并行推进：

- A 负责 `libge_python_pass_bridge.so`、`pass_plugin_loader`、`ge_compiler.so` 稳定 ABI、adapter 路由、fallback 装载，以及现有 `ge.graph.Graph` 的 borrowed view 接入
- B 负责 `_ge_pass_native.so`、`base.py`、`PassContext` / `MatchResult` 的 native-backed wrapper，以及 Python sample / Python API 的补齐

B 需要明确交付：

- `_ge_pass_native.so` 的构建脚本与模块导出
- `PassContext` borrowed view wrapper
- `MatchResult` 最小可用 wrapper
- 必要的 helper / 工厂接口，供 `libge_python_pass_bridge.so` 构造 Python 对象
- `base.py` / `pattern.py` 中 `PassContext` / `MatchResult` / `Pattern` / `PatternMatcherConfig` 的 native-backed 直接导出
- Python sample 和 Python API 补齐所需的最小能力清单

A 需要明确交付：

- `graph.py` 中 `Graph._create_from(handle, owns_handle, owner)` 的 borrowed / non-owning 语义
- `python_fusion_base_pass_pybind_bridge.cc` 中 `BuildPythonGraph()` 对现有 `ge.graph.Graph` 的正式接入
- `libge_python_pass_bridge.so` 与 `_ge_pass_native.so` 的桥接集成点

关于 `Graph` 这条边界，需要特别固定如下原则：

- `Graph` 优先复用当前已经存在的 `ge.graph.Graph`
- `_ge_pass_native.so` 不再引入第二套用户可见的 `Graph` 类型
- A 负责把 runtime `GraphPtr` 以 borrowed view 方式接到现有 `ge.graph.Graph`
- B 不直接拥有 `Graph` 类型本身，而是围绕 `PassContext` / `MatchResult` / helper 提供配套能力

B 完成后，`base.py` 应收敛为：

- `FusionBasePass` / `PatternFusionPass` / `DecomposePass` 仍然保持纯 Python 基类
- `PassContext` / `MatchResult` / `PatternMatcherConfig` 直接 re-export `_ge_pass_native` 提供的类型
- `Pattern` 通过 `ge.passes.pattern` 直接导出 `_ge_pass_native` 提供的类型
- 不再为 `_ge_pass_native` 缺失场景保留兼容 shim

A 在本地拆分 bridge `.so` 与 loader 时，阶段 1 可先不依赖 `_ge_pass_native` 做最小验证，但这只是一条临时 bring-up 策略，不应继续保留到阶段 2 收口后的正式代码中：

- 先继续使用 `FusionBasePass` 现有纯 Python 合同类
- `_bridge.py` 与 bridge `.so` 先验证 descriptor、holder、create/run/destroy、`dlopen`、fallback 产物选择
- 涉及 `PatternFusionPass` 的正式端到端验收，等 B 的 `_ge_pass_native` 落地后再合流完成

补充约束：

- A 不拥有 `base.py`，只消费 B 暴露出来的稳定 Python 接口
- B 不直接拥有 `libge_python_pass_bridge.so`，只提供 bridge 需要使用的稳定 Python / native helper 能力

### 12.6 ATC 参数接入

修改如下文件：

- `api/atc/main_impl.cc`

职责如下：

- 当前不新增新的用户 pass 路由参数
- 后续若补 CLI / option，也应最终归一到 `ASCEND_GE_PY_PASS_PATH` 或 `ge.passes.bootstrap` 的统一发现协议

### 12.7 文档、样例、测试

新增和修改如下文件：

- `examples/fusion_pass/README.md`
- `examples/fusion_pass/python_pass开发指南.md`
- `examples/fusion_pass/*/python/*`
- `tests/ge/ut/ge/graph/pyge_tests/*_test.py`
- `tests/ge/python_pass/*`

职责如下：

- 给出 Python sample
- 给出用户使用文档
- 完成单测与集成验证

## 13. 协作与推进方式

说明：

- 本设计文档负责维护长期稳定的架构边界、A/B 协作约束、接口冻结点和验收原则
- `PLAN.md` 是唯一的阶段进展、checklist 与完成状态来源
- 阶段目标、A/B 子目标、已完成/未完成状态后续只在 `PLAN.md` 更新；本设计文档不再重复维护同类进度信息

### 13.1 协作总原则

建议持续按两条并行工作流推进，但以“接口先冻结、写集尽量分离、阶段状态统一回写 `PLAN.md`”为基本原则。

并行协作遵循以下约束：

- 先冻结公共接口，再并发开发：
  - `ge.passes._bridge` 与 native bridge 之间的 descriptor / callback 协议
  - `ge.graph` / `ge.es` / `_ge_pass_native.so` 对 Python 可见的最小接口
- 进度跟踪统一维护在 `PLAN.md`
- 设计文档只保留长期有效的边界，不保留“谁完成了哪一项”的过程性记录
- 阶段验收可以按阶段逐步执行，但是否“阶段完成”以 `PLAN.md` 的 checklist 为准

### 13.2 A/B 工作流边界

两条工作流的长期边界如下：

- A 聚焦 compiler / native bridge / loader / adapter / fallback / 现有 `ge.graph.Graph` 接入
- B 聚焦 `_ge_pass_native.so`、`base.py`、`PassContext` / `MatchResult`、Python sample 与 Python API 补齐

与当前实现直接相关的固定边界如下：

- A 负责 `libge_python_pass_bridge.so`、`pass_plugin_loader`、`ge_compiler.so` 稳定 ABI，以及现有 `ge.graph.Graph` 的 borrowed view 接入
- B 负责 `_ge_pass_native.so`、`base.py`、`PassContext` / `MatchResult` 的 native-backed 形态，以及 sample / Python API 的后续补齐
- `Graph` 优先复用当前已经存在的 `ge.graph.Graph`
- `_ge_pass_native.so` 不再引入第二套用户可见的 `Graph` 类型
- A 不拥有 `base.py`
- B 不拥有 `libge_python_pass_bridge.so`

### 13.3 阶段推进与交付方式

后续阶段按下面的推进顺序协作：

1. 先冻结阶段边界与完成定义，并写入 `PLAN.md`
2. 再冻结 A/B 之间的最小接口
3. 各自按写集并行开发
4. 合流时优先完成测试内 sample 和定向验证
5. 通过后把 checklist 状态更新回 `PLAN.md`

对阶段定义本身，建议保持以下长期顺序：

- `FusionBasePass` 正式打通
- `PatternFusionPass` 打通
- `DecomposePass` 打通
- fallback 与预制版本打通
- sample 的 Python 等价实现
- 配套文档、验证与交付资料完成

这些阶段的详细子项、状态和阻塞项统一引用 `PLAN.md`，不在设计文档中再次展开。

### 13.4 阶段验收与文档同步

每个阶段收口时，建议同步完成以下动作：

- 更新 `PLAN.md` 中对应 checklist 的完成状态
- 若接口边界发生变化，再更新本设计文档
- 若只是状态变化，不再修改本设计文档中的阶段说明
- 不再新增独立的阶段进展/验收 markdown；过程性进展统一回写 `PLAN.md`
- 为阶段验收沉淀最小验证命令、结果摘要和已知阻塞项

这样可以保证：

- `PLAN.md` 反映实时进度
- 设计文档保持稳定，不被过程性状态污染
- A/B 在联调时始终有唯一的进展来源

## 14. 验证与验收要求

### 14.1 测试分层

V1 的测试建议分四层组织：

- Python 单测：
  - `registry`、`decorator`、`bootstrap`
  - 环境变量主路径、后续 `entry_points` 自动发现
  - descriptor 规范化
  - borrowed handle 的 stale 检查
- C++ / native 单测：
  - bridge 初始化与重复初始化
  - TLS 创建上下文
  - 动态注册
  - session 生命周期
  - 异常隔离
- 集成测试：
  - `FusionBasePass`
  - `PatternFusionPass`
  - `DecomposePass`
  - `MatchResult`
  - `GeUtils.InferShape`
  - `GeUtils.CheckNodeSupportOnAicore`
- 打包与安装测试：
  - 主 wheel 安装
  - native 子 wheel 选择
  - 无匹配版本时 fallback 编译
  - 开发态路径 / 模块直传

### 14.2 阶段验收原则

每个阶段的完成定义、A/B 子目标、状态和阻塞项统一以 `PLAN.md` 为准，本设计文档不再重复维护“阶段一/阶段二/阶段三”的硬编码验收节点。

阶段验收建议统一采用以下结构：

- 完成定义：
  - 直接引用 `PLAN.md` 中对应阶段的“阶段完成定义”
- 必过验证：
  - 至少覆盖一条正向主链路
  - 至少覆盖该阶段新增接口的异常路径或失败隔离
  - 至少覆盖生命周期、重复加载或资源释放中与该阶段直接相关的部分
- 收口要求：
  - `PLAN.md` 对应 checklist 状态完成更新
  - 设计文档中受影响的接口边界、装载关系或职责分工完成同步
  - 阶段验收记录中保留验证命令、结果摘要和已知阻塞项

另外需要遵循一个约束：

- 某项能力属于哪个阶段，就在哪个阶段验收，不提前并入其他阶段
- 例如 `entry_points`、预制版本、多版本 native 产物、fallback 等产品化能力，应以 `PLAN.md` 中对应阶段为准，而不是提前写入前置阶段的完成标准

### 14.3 里程碑组织建议

考虑到项目以“两周”为节奏推进，正式里程碑建议按 `PLAN.md` 当前优先级动态组织，而不是在本设计文档中写死“某两个阶段必须绑在一起验收”。

建议每个里程碑遵循以下原则：

- 一个里程碑尽量只收口 `1~2` 个可以形成闭环的主题
- 优先围绕当前主目标组织，例如：
  - `FusionBasePass` 闭环
  - `PatternFusionPass` 闭环
  - `DecomposePass` 闭环
  - fallback / 预制版本闭环
  - sample 与交付资料闭环
- 每个里程碑都应输出：
  - 本轮完成的 checklist delta
  - 定向验证命令与结果摘要
  - 仍然存在的阻塞项
  - 下一里程碑的交接前提

扩展项建议单独组织里程碑，不与 V1 主链路验收强绑定。例如 `REGISTER_CUSTOM_PASS` 更适合在主链稳定后单独验收。

### 14.4 建议验收输出物

每次正式验收建议至少沉淀以下输出物：

- 测试结果汇总表：
  - 用例名称
  - 覆盖能力点
  - 执行结果
  - 失败原因与结论
- sample 执行记录：
  - 输入模型或脚本
  - 触发的 pass
  - 关键日志或结果摘要
- 已知问题列表：
  - 是否阻塞下一阶段
  - 规避方式
  - 计划修复阶段

### 14.5 总体验收维度

V1 最终建议按以下维度做总体验收：

- 发现与装载：
  - 当前纳入范围的发现机制与 `PLAN.md` 一致，并且与用户文档口径一致
  - 若本轮纳入 `entry_points`、预制版本或 fallback，则对应链路具备独立验收记录
- 三类 pass 主链：
  - `FusionBasePass`、`PatternFusionPass`、`DecomposePass` 在各自纳入范围内可发现、注册、创建和执行
- Python / native wrapper：
  - `Graph`、`PassContext`、`MatchResult` 以及阶段所需 helper 具备对应能力
- 稳定性：
  - Python pass 导入失败可隔离
  - Python pass 执行异常可隔离
  - 生命周期、重复加载、资源释放语义具备验证记录
- 交付与资料：
  - `PLAN.md`、设计文档、sample、验证记录和限制说明保持一致

### 14.6 阶段进展引用

当前阶段完成状态、A/B 子目标、未完成项和阻塞项不在本设计文档中重复维护，统一以 `PLAN.md` 为准。

本设计文档只保留以下长期验收要求：

- 每个阶段收口时必须同步更新 `PLAN.md`
- 若接口、职责边界或装载关系发生变化，必须同步更新本设计文档
- 若只是任务状态变化，只更新 `PLAN.md`
- 阶段验收需要沉淀验证命令、结果摘要和已知阻塞项

## 15. 风险与关注点

- `CreateFusionPassFn` 如果直接改成 `std::function`，可能引入 `dlclose` 与全局析构顺序耦合风险，优先采用“函数指针 + TLS 创建上下文 + 进程级注册表”的方案。
- `Graph` borrowed wrapper 已经落地，但后续新增运行时 wrapper 仍必须遵守“默认不接管 runtime 句柄所有权”的约束，避免再次引入双释放风险。
- 本地开发环境是 Python 3.13，而正式 wheel 规划是 `cp39-cp314`，本地只能覆盖当前环境可用的 Python tag，完整矩阵需要发布流水线按 Python minor 版本分别验证。
- V1 需要在文档和实现中明确区分两类 native 策略：`ge.graph` 继续沿用 ctypes/C wrapper，Python pass bridge 采用 `pybind11`。
- 如果继续把更多 Python 版本敏感逻辑直接编进 `ge_compiler.so`，后续 bridge artifact set 与 fallback 的演进空间会被压缩；因此后续实现必须把版本差异收敛到可替换的 `libge_python_pass_bridge.so` / `_ge_pass_native.so` 中。
- `atc.bin` 内可能由 TBE 先初始化 Python，也可能由 Python pass bridge 先初始化 Python；fallback 选择必须以当前进程内解释器或统一 selector 为准，并把 bridge 状态清理与解释器 finalize 拆开设计，保证所有 Python 使用方都清理完之后才由 owner finalize。
- `PatternFusionPass` 的 Python 化不是简单函数回调，必须保证 pattern/match/replacement 三段语义与现有 C++ 框架一致。
- 在 B 的 `_ge_pass_native` 落地前，本地验证可以先不依赖它，但这只能用于独立 bridge `.so` 拆分与 `FusionBasePass` 回归；不能替代 `PatternFusionPass` 的最终验收。
- 9 个 sample 全部提供 Python 对照版会扩大 wrapper 覆盖面，需要优先保证主链路可用，再逐步补齐边角接口。

## 16. 与后续自定义算子 Python 化的复用边界

本方案不只是为 Python pass 单点服务，更是在铺设“GE/CANN 主框架安全调用 Python 扩展能力”的通用底座。

后续如果推进“自定义算子 Python 化”，本方案中的一大部分基础设施可以直接复用，但算子定义、交付和下沉相关能力仍然需要新增专属设计。

### 16.1 可直接复用的能力

以下能力后续可以直接复用到自定义算子 Python 化中：

- Python 插件发现协议：当前以环境变量主路径为基线，后续再补 `entry_points` 自动发现，这套协议可以抽象成通用的 Python 扩展发现框架。
- 主 wheel + native 子 wheel + fallback codegen：这一套“纯 Python 主包 + 多 Python 版本 native 伴生包 + 未命中版本本地兜底编译”的发布模式，本质上与 pass/custom op 无关。
- bridge 生命周期管理：Python 解释器初始化、重复初始化幂等、异常隔离、卸载顺序控制、日志与诊断都可以复用。
- GIL 与锁策略：当前文档中定义的“管理锁 + session 轻量状态 + GIL 精确包围 Python 回调”的策略，适用于后续绝大多数 Python 扩展接入点。
- 执行期 session / holder 模型：运行期对象和 Python wrapper 的 owner token、stale handle 检查、borrowed 对象失效后转 Python 异常而不是崩溃，这一套机制同样适用于自定义算子回调期对象。
- Python 注册表与 descriptor 模型：当前 pass 设计中的 registry、descriptor、bootstrap、bridge 出口可以演化为通用“Python extension registry”。
- Pythonic 约束：不要求用户手动释放对象、不要求手动管理 GIL、不要求用户显式 close/release 的体验目标，后续应继续保持。

如果只看“Python 能力正式接入 GE/CANN”的底座层，这一层的复用度预估可达到 `60%~70%`。

### 16.2 可部分复用的能力

以下能力可以复用一部分，但需要根据自定义算子的语义重新裁剪：

- `ge.graph` / `ge.es` wrapper：如果后续自定义算子 Python 化需要表达 schema、infer 逻辑、图级 replacement 或 eager 入图过程，这些 graph wrapper 仍然有价值；但它们不能替代算子 schema 自身的建模。
- 插件路径与安装路径管理：当前仓已经存在 custom opp 路径和 plugin manager 能力，例如 `graph_metadef/base/common/plugin/plugin_manager.cc` 中的 `custom_op_lib_path_` 相关逻辑，这说明“插件发现”和“自定义交付件安装路径”有现成基础；但 Python custom op 还需要定义更明确的 Python 包和 OPP 包之间的映射规则。
- 错误传播与降级策略：当前 pass 里的“单插件失败不拖垮主链路”思路仍然适用，但 custom op 往往比 pass 更靠近编译主流程，哪些错误允许降级、哪些错误必须 hard fail，需要重新分级。
- ATC 参数入口：本方案里为 `--py_pass_path`、`--py_pass_module` 预留的参数接入方式，后续可扩展到 Python custom op；但参数命名、优先级和与现有 custom opp 目录配置的关系还需要再定。

这一层更像“机制复用”，不是“实现直接拷贝”。复用比例大致在 `20%~30%`。

### 16.3 需要为自定义算子新增的能力

以下部分基本不能直接从 Python pass 方案里获得，后续需要单独设计：

- 自定义算子的 schema/OpDef 注册模型：包括输入输出、属性、约束、默认值、版本、命名空间。
- shape/type inference 的 Python 注册与执行模型。
- kernel 交付链路：例如 AscendC/Triton/TBE/host 侧实现、编译产物、二进制布局、版本校验。
- OPP 包布局与安装协议：Python 包、op proto、kernel 二进制、tiling 文件、配置文件之间的目录组织关系。
- 编译/运行时识别逻辑：ATC 和在线编译阶段如何发现、校验、下沉 Python custom op。
- 自定义算子与 framework adaptor 的衔接：例如当前 `examples/custom_op` 中展示的 PyTorch/TensorFlow 入图方式，这部分明显超出 pass 的职责范围。

如果后续目标只做到“Python 写 schema + infer + 注册”，那么当前 pass 方案带来的复用会更高；如果目标还包括“Python 侧完整驱动 kernel 开发、打包、发布”，新增工作量会明显增大。

### 16.4 复用结论

建议把“Python pass 化”和“Python custom op 化”看成两个阶段：

- 第一阶段先完成 Python pass，把 Python 插件接入、生命周期、内存管理、锁/GIL、打包与 fallback 这些通用底座做稳定。
- 第二阶段在这个底座上补自定义算子专属模型，包括 schema、infer、kernel 交付和 OPP 布局。

从项目维度粗估：

- 基础设施层复用：`60%~70%`
- 项目整体复用：`40%~50%`

这个比例已经足够说明，当前 Python pass 设计不是一次性方案，而是在提前建设后续 Python custom op 可以持续复用的公共底座。
