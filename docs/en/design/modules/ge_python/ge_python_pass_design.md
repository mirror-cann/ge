# GE Pass Python Implementation V1 Design Document

## 1. Background

GE currently has two types of basic capabilities directly related to this requirement:

- The existing custom pass loading chain already exists, GE will discover and `dlopen` pass libraries through `opp/vendors/*/custom_fusion_passes/*.so`.
- Python side already has `ge.es` graph construction capability and `ge.graph` basic graph interface.

The goal of this design is to introduce formal Python pass development capability without overturning the existing GE pass execution framework, so that users can both quickly develop and debug locally and distribute passes as standard Python packages to teams.

## 2. Goals and Scope

### 2.1 Goals

- Support users to develop GE passes using Python.
- Reuse existing GE pass execution chain, do not add a second pass scheduling framework.
- Reuse existing `ge.es` Python graph construction capability, do not redesign Python ES.
- First support environment variable-driven development mode integration, then supplement release mode auto-discovery.
- Reserve extension points for subsequent Python ATC entry to reuse the same pass registration and discovery protocol.

### 2.2 V1 Scope

- Support three types of passes:
  - `FusionBasePass`
  - `PatternFusionPass`
  - `DecomposePass`

- The bring-up and independent bridge separation phase still uses `FusionBasePass` regression as the minimum verification chain; after formal wrapper implementation, the first complete acceptance target shifts to `PatternFusionPass`, with `DecomposePass` added afterwards, but still within V1 scope of this design document.

- Current phase discovery mechanism is first converged to:
  - Environment variable `ASCEND_GE_PY_PASS_PATH`

- Subsequent phase will add:
  - `entry_points(group="ge.passes.plugins")`

- Current code has converged to the environment variable main path, with `entry_points` as subsequent phase capability supplement.

- Complete minimum graph interface capabilities required for Python pass writing.
- Current repository directly outputs `ge_py` main wheel and multi-version native sub-wheels.
- Existing 9 samples in `examples/fusion_pass` are planned to provide Python comparison versions.
- `REGISTER_CUSTOM_PASS` needs to be supported, but not as the first main chain target, placed in subsequent extension phase based on the same bridge / registry / session mechanism reuse implementation.
- Current priority rectification items:
  - Discovery mechanism first converged to environment variable `ASCEND_GE_PY_PASS_PATH`
  - `entry_points` to be added later
  - `python_pass_bootstrap_test.py` migrated to `tests/ge/ut/ge/graph/pyge_tests/` and connected to current frontend script
  - New file year uniformly uses `2026`, existing old files do not batch change year
  - First priority is to target "formal sample of `FusionBasePass` end-to-end passing via environment variable", all capabilities involved in this goal must be formally completed, capabilities not involved can be deferred
  - After phase 2 closure, `PassContext` / `MatchResult` / `Pattern` / `PatternMatcherConfig` formal Python form uniformly directly depends on `_ge_pass_native.so`, no longer retaining bring-up phase compatibility shim

### 2.3 Non-Goals

- V1 does not force users to package passes into whl.
- V1 first batch does not take legacy `REGISTER_CUSTOM_PASS` system as the main Python implementation object, prioritizing coverage of `PassRegistry` system's three pass types; but architecture and documentation need to reserve subsequent integration capability.
- V1 does not create a second Python-only pass executor.

## 3. User Experience Design

### 3.1 Development Mode

After users install the `ge_py` provided by CANN, they only need to write ordinary `.py` files or ordinary Python packages and tell GE/ATC via environment variables:

- `ASCEND_GE_PY_PASS_PATH=/abs/path1:/abs/path2`

The current phase focuses on stabilizing this path first, not requiring users to write their own wheel packaging logic, nor requiring users to understand `entry_points`.

### 3.2 Release Mode (Subsequent Phase)

When users need team sharing, version freezing, and auto-discovery, they can make passes into independent Python packages and declare:

- `entry_points = {"ge.passes.plugins": [...]}`

GE automatically discovers installed pass plugin packages at runtime.

### 3.3 Notes

The current phase environment variable is not a fallback, but the main path:

- `ASCEND_GE_PY_PASS_PATH=/abs/path1:/abs/path2`

`entry_points` auto-discovery to be added later.

## 4. Overall Architecture

### 4.1 Architecture Principles

- GE collects pass plugin loading through a unified upper-level loader during initialization phase.
- Legacy custom passes continue to use the existing `.so + dlopen` mechanism.
- From long-term productization and extensibility perspective, Python pass bridge should be designed as "independent internal bridge `.so`" from the beginning, rather than directly compiled into `ge_compiler.so`.
- This still does not adopt the "bridge `.so` discovered and loaded by `custom_fusion_passes` as a pass plugin" approach; the recommendation is a private bridge `.so` explicitly loaded by GE internal loader.
- The design goal is to keep `ge_compiler.so` as Python ABI neutral as possible, only retaining stable pass runtime, registry and adapter protocols; all logic directly depending on `Python.h` / `pybind11` / `libpython` should be converged into replaceable independent bridge `.so`.
- This way, whether going through pre-compilation or fallback codegen, the replacement target is the independent bridge `.so`, not the `ge_compiler.so` in the run package.
- Python side uniformly manages plugin discovery, module import and registry through `ge.passes.bootstrap`.
- C++ side only cares about "getting executable pass descriptor and registering to PassRegistry", not about where user Python files specifically are.

### 4.2 Core Components

- `PassPluginLoader`
  - Unified pass plugin loading entry at compiler layer
  - Internally uniformly calls legacy `CustomPassHelper::Load()` and Python pass registration logic
  - Maintains "one call entry closure", while not putting Python logic back into `graph_metadef/register`

- `ge.passes.bootstrap`
  - Python-side unified discovery entry
  - Currently prioritizes environment variable discovery, with `entry_points` to be added later

- `ge.passes.registry`
  - Python-side registry
  - Responsible for storing pass metadata, class objects, stages, types, and additional parameters

- `ge.passes._bridge`
  - Protocol layer between Python and C++ bridge
  - Responsible for normalizing Python registry objects into C++ consumable data structures

- `_ge_pass_native`
  - Python helper module exported by `PYBIND11_MODULE`
  - Only carries `Graph` / `PassContext` / `MatchResult` and other native-backed wrappers and helpers
  - Does not carry `FusionBasePass` / `PatternFusionPass` / `DecomposePass` these user-inheritable pass base classes

- C++ pass adapter
  - Provides corresponding C++ adapter classes for three types of Python passes
  - Calls back Python object methods in adapter classes

- Independent bridge `.so` loaded as pass plugin via `dlopen`
  - Current main approach has explicitly abandoned this
  - Any description in the document based on "adding a new bridge `.so` discovered by `custom_fusion_passes`" should be understood as "unified loader + private internal bridge `.so`", not a new pass plugin discovery chain

- Private internal bridge `.so`
  - This is the recommended formal direction in this design, not an optional optimization
  - It is not a pass plugin and does not participate in `custom_fusion_passes` discovery; it carries complete Python version-sensitive bridge logic, including interpreter initialization, GIL, object conversion, exception translation, and Python callbacks
  - During formal delivery, it forms the same bridge artifact set with `_ge_pass_native.so`, both need to be included in pre-compilation and fallback management

### 4.3 Native Binding Strategy

V1 adopts a "two-layer binding" strategy, rather than migrating all Python interfaces to the same binding method at once:

- `ge.graph` / `ge.es`
  - Continue to reuse the existing C wrapper + `ctypes` approach in the repository
  - This minimizes changes and allows prioritizing reuse of existing Python graph interfaces and eager-style graph construction capabilities

- Python pass bridge / adapter layer
  - Uses `pybind11` as core implementation strategy
  - Reason is that this part needs to more naturally handle `MatchResult` wrapping, Python object lifecycle, exception translation, and GIL management

- Version release strategy
  - Main wheel is responsible for Python code, discovery logic, and runtime selection logic
  - `cp39-cp312` provides pre-compiled `pybind11` native sub-wheels
  - When pre-compiled version is not matched, runtime fallback codegen as final fallback

That is, V1 is not "full pybind migration", but "graph interfaces continue with existing ctypes/C wrapper, pass bridge adopts pybind11".

### 4.4 pybind11 Usage Method Selection

`pybind11` has two typical usage methods in this solution:

- embed mode
  - Python interpreter initialized by C++ process, `import` Python modules, and callback Python objects

- extension mode
  - Export C++ capabilities as Python-directly `import`-able native modules through `PYBIND11_MODULE`

The existing `compiler/graph/fusion/pass/python_fusion_base_pass_pybind_bridge.cc` uses embed mode, not extension mode. The reasons are:

- Current main direction is "GE compiler calls Python pass", not "Python actively imports a C++ pass runtime then reversely drives compiler"
- `FusionBasePass` first stage only needs to let C++ safely create Python objects and call their `run()`, without first exposing C++ base classes to Python for inheritance
- embed mode can first reuse existing `ge.passes.bootstrap / registry / _bridge` pure Python organization, reducing first implementation cost

Therefore, the current bridge file will not have `PYBIND11_MODULE` macro, this is not a missing feature, but an intentional mode difference.

But from long-term design perspective, embed bridge should not continue to be directly compiled into `ge_compiler.so`. Current workspace has already split it into independent `libge_python_pass_bridge.so`, which is also the formal boundary that should be continuously maintained. A more reasonable form is:

- `ge_compiler.so`
  - Only holds stable loader, descriptor / adapter protocols, and minimal C/C++ interaction surface

- Independent internal bridge `.so`
  - Adopts embed or extension whichever is more suitable for specific implementation
  - Undertakes all Python version-sensitive native logic

- `_ge_pass_native.so`
  - As Python-directly `import`-able helper extension
  - Provides native-backed wrappers and helpers for Python layer, but does not define user-inheritable pass base classes

What needs to be further emphasized is that the current solution has two clear boundaries that cannot be mixed:

- User pass definition layer
  - Continues to maintain pure Python form, users inherit `ge.passes.base.FusionBasePass` / `PatternFusionPass` / `DecomposePass`

- native helper / wrapper layer
  - Provided by `_ge_pass_native.so` for `Graph` / `PassContext` / `MatchResult` and other wrappers
  - Provided by `libge_python_pass_bridge.so` for embed path runtime bridging

That is, this solution does not require or recommend exposing C++ `FusionBasePass` / `PatternFusionPass` base classes to Python for inheritance through `PYBIND11_MODULE`.

What needs special explanation is that Python version sensitivity is not only present with `PYBIND11_MODULE`. As long as native code directly depends on:

- `Python.h`
- `pybind11`
- `libpython`

Whether embed or extension, it will naturally have binary coupling with Python minor version. `PYBIND11_MODULE` is just the export entry for extension mode, not the root cause of version sensitivity.
9. bootstrap discovers and imports user pass modules
10. User pass modules register pass to `ge.passes.registry` through decorators
11. Bridge reads registry and dynamically registers descriptor back to `PassRegistry` through registrar callback

Corresponding call relationship can be simplified为 following timeline:

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

Where:

- `registrar` is constructed by loader, representing "how to register descriptor back to compiler"
- `bridge` is responsible for discovering Python pass, and after getting descriptor, callback `registrar`
- `RegisterPythonFusionBasePass(...)` is真正 landing point that挂 descriptor, callbacks and creator back to compiler侧 registration center

Phase 1 split初期, `libge_python_pass_bridge.so` can first only depend on pure Python `bootstrap / _bridge` protocol to complete minimum integration调试; Phase 2 closure后的 formal approach则要求 `_ge_pass_native.so` simultaneously到位, bridge and Python API default基于 same set of native helper 运行.

### 5.2 Execution Phase

1. `FusionPassExecutor` gets pass from `PassRegistry` according to现有 flow
2. If pass is Python pass, then actually created is corresponding C++ adapter instance
3. Adapter calls back Python pass object in `Run` or related phase
4. Python pass reads/writes graph and builds replacement graph through `ge.graph` and `ge.es` interfaces
5. Return value maps为 GE `Status`

## 6. Python Public Interface Design

### 6.1 Package Structure

Add new `ge.passes` package, providing following public interfaces:

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
- `capture_tensor`
- `create_pattern`
- `create_replacement`
- `FuseCheckResult`
- `can_fuse`
- `report_fuse`
- `load_pass_plugins`
- `get_registered_passes`

### 6.2 Method Style

Default use Python style naming

### 6.3 Registration Interface

Suggested form如下:

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

### 6.4 Python-side Return Value Convention

- `run` returns `StatusLike`
- `meet_requirements` returns `bool`
- `patterns` returns `list[Pattern | Graph]`
- `replacement` returns `Graph`
- If希望 skip current match, must return `False` in `meet_requirements`, not支持通过 `replacement` returning `None` to express "abandon replacement"


Where `StatusLike` in Python layer uniformly converts为 GE `Status`.

## 7. Discovery Mechanism Design

### 7.1 Unified Entry

Python side uniformly provides:

- `ge.passes.bootstrap.load_pass_plugins()`
- `ge.passes.bootstrap.get_registered_passes()`

bridge registration chain路 before each round loading first由 C++ according to current process environment refresh Python `os.environ`中 `ASCEND_GE_PY_PASS_PATH`,避免 resident Python interpreter中 environment cache affecting下一轮 pass discovery.

### 7.2 Discovery Priority

Current phase priority converges to:

1. Environment variable `ASCEND_GE_PY_PASS_PATH`

Subsequent phase will add:

2. `entry_points(group="ge.passes.plugins")`

### 7.3 Environment Variable Mode

- `ASCEND_GE_PY_PASS_PATH` supports multiple directories,以 `:` separated
- Directory中 allows single file module or ordinary Python package
- bootstrap负责将这些 directories temporarily add to `sys.path`

### 7.4 entry_points Mode (Subsequent Phase)

- group fixed为 `ge.passes.plugins`
- value can point to module path, or return module's callable
- Module after import通过 decorator completes registration

## 8. Three Types Pass Bridge Design

### 8.1 FusionBasePass

Most direct type, C++ adapter calls Python object:

- `run(graph, context)`
- Return value constraint为 `None` / `bool` / `int` three types status value
- Formal pass contract中, `context`始终为 `PassContext`
- Only `_bridge.py`'s direct bridge/pytest auxiliary entry allows passing `None`

该 type优先打通,作为整条链路's minimum closure.

### 8.2 PatternFusionPass

该 type continues reusing existing C++ PatternMatcher mechanism. Python side only负责:

- Providing pattern graph
- According to `MatchResult` judging whether满足 condition
- Constructing replacement graph

This means C++ side needs a Python adapter继承 `PatternFusionPass`, in following points callback Python:

- `Patterns()`
- `MeetRequirements()`
- `Replacement()`

Here有一个明确方案 constraint:

- Not requiring Python user class directly inherit一个通过 `PYBIND11_MODULE` exposed C++ `PatternFusionPass`
- User continues inheriting pure Python's `ge.passes.base.PatternFusionPass`
- Reusing C++ base class public `Run()` flow's responsibility放在 `PythonPatternFusionPassAdapter`上,而不是放在 Python user class上
- Python subclass禁止 override `run()`; if误 override, base class in class definition phase directly throws `TypeError`,避免 "implemented but永远不会被调用" ambiguity

Recommended form是:

- `PythonPatternFusionPassAdapter : public PatternFusionPass`
- adapter overrides `Patterns()` / `MeetRequirements()` / `Replacement()`
- Override functions内部再 callback Python pass instance
- `Run()` directly reuses existing C++ `PatternFusionPass::Run()`

Choosing this approach reason是:

- 能最大化 reuse existing C++ `PatternMatcher`, rewrite, statistics and error handling logic
- Not forcing Python user environment must first successfully import一个 native C++ base class module, keeping `ge.passes` pure Python API usability
- 能让 `FusionBasePass`, `PatternFusionPass`, `DecomposePass` three types pass在 user side keep unified style
- 能把 Python version sensitive问题尽量收敛 in adapter / wrapper / native bridge layer,而不是扩散 to user pass base class definition layer

### 8.3 DecomposePass

该 type continues reusing existing `DecomposePass` semantics. Python side only负责:

- `MeetRequirements(const GNode &)`
- `Replacement(const GNode &)`

Construction时 needs retaining `op_types` information.

与 `PatternFusionPass` same, Python user class不 directly接管 `Run()` main flow,而是只 implement hooks:

- `meet_requirements(node) -> bool`
- `replacement(node) -> Graph`

Here额外 contract constraint needs明确写死 in Python base class里:

- Python subclass禁止 override `run()`; if误 override, base class in class definition phase directly throws `TypeError`
- `replacement()` must return replacement Graph
- If希望 skip current node,必须在 `meet_requirements()` phase return `False`,不支持通过 `replacement()` return `None`

与 `PatternFusionPass` same,这里也不要求 Python user class directly inherit pybind exposed C++ `DecomposePass`. Recommended form是:

- `PythonDecomposePassAdapter : public DecomposePass`
- adapter在 C++ side reuses `DecomposePass::Run()`
- adapter overrides `MeetRequirements()` / `Replacement()` and转调 Python pass instance

This可以 retain existing C++ `DecomposePass`'s main flow semantics,同时避免把 construction parameters, `op_types` and Python version sensitive logic directly expose给 Python base class inheritance system.

### 8.4 creator and Context Acquisition Design

Current `CreateFusionPassFn`是 naked function pointer:

- `using CreateFusionPassFn = FusionBasePass *(*)();`

V1不建议把它直接改成 `std::function<FusionBasePass *()>`. Reasons有两点:

- Python pass来自 bridge `.so`'s dynamic registration, if creator holds可捕获 lambda,析构链容易和 `dlclose` order coupling
- Once `PassRegistry` or other global object在 bridge `.so`已 unloaded后析构, `std::function` internal object析构就可能访问已 unloaded code,存在 `coredump` risk
Here当前不再以 "独立 bridge `.so`'s `dlclose` risk"作为 main reason. More accurate reason是:

- Existing creator ABI仍是无参 naked function pointer,直接改成可捕获 object会扩大 impact面
- `std::function`会把 runtime routing information, object析构 and call path耦合进 creator本身,不利于 maintaining "creator只做最小 routing, runtime resources放在 bridge/runtime registry" layering
- Retaining naked function pointer + TLS routing context,仍是当前影响面最小、最稳妥方案

Therefore here建议采用更稳妥方案:

- Retain `CreateFusionPassFn`为 naked function pointer
- Add一个 "creation phase TLS context"
- Python pass's runtime object and metadata放在 bridge held process-level registry中

Here说 "identification information"不是 Python runtime context itself,而是**用于 registry lookup's stable key and metadata**,例如:

- `pass_name`
- `pass_kind`,即 `fusion` / `pattern` / `decompose`
- `stage`
- Python module name
- Python class full name
- `decompose` scenario下 `op_types`

These information属于 registration phase static information,不是 execution phase context.真正 Python interpreter state, module object, pass instance,不放在 creator里携带,而是放在 bridge's global registry中 unified management.

Recommended implementation方式如下:

1. 在 consume `create_fn()`'s position set TLS creation context
- 最终建议传入 `descriptor_key`
- 某些现有 call points短期更容易拿到 `pass_name`时,可先做过渡 mapping,但不建议把 `pass_name` fixed化为最终 creator routing key

2. 为三类 Python pass提供少量 generic creator functions
- `CreatePythonFusionPass()`
- `CreatePythonPatternPass()`
- `CreatePythonDecomposePass()`

3. Generic creator从 TLS中读取 current `descriptor_key`
- 再据此从 bridge registry中找到对应 descriptor
- Then construct对应 adapter

4. Adapter在 execution时再从 bridge registry获取 Python pass instance or its holder

This design's advantages是:

- 不需要把 Python object context塞进 `create_fn`
- 不引入 `std::function`'s析构顺序 risk
- 与当前 GE's creator call方式 compatibility更好

### 8.5 TLS Creation Context Refinement

Current implementation需要保留一个轻量 creation phase TLS context. Its necessity不是 "为了传更多信息",而是因为 current `FusionPassRegistrationData::CreatePassFn`仍是无参 creator,而 Python pass side多个 descriptors会共用同一个 adapter factory;如果没有这份 TLS, upper layer `PassRegistry::CreatePass()` although knows "正在创建哪个 pass", shared factory却不知道应该绑定哪个 Python descriptor.

Current code form可表示为:

```cpp
struct PythonPassCreateContext {
  std::string descriptor_key;
  std::string pass_name;
  PythonPassKind kind;
};
```

并提供如下 auxiliary facilities:

- `SetCurrentPythonPassCreateContext(descriptor_key)`
- `GetCurrentPythonPassCreateContext()`
- `ClearCurrentPythonPassCreateContext()`
- RAII scope guard,例如 `PythonPassCreateScope`

Usage方式如下:

1. 在调用 `create_fn()`前,由调用方设置当前 `descriptor_key`
2. Generic creator读取 TLS中 `descriptor_key`
3. Generic creator再到 bridge registry中查找对应 descriptor
4. 构造对应 adapter
5. `create_fn()`返回后自动清理 TLS

其中:

- `descriptor_key`是真正 routing primary key
- `pass_name` / `kind`当前主要承担一致性校验,避免 shared factory误绑定到错误 descriptor

后续如果调用链进一步统一,仍建议把 `PythonPassCreateContext`收敛到 "只保留 `descriptor_key`这一最小必要字段",避免状态重复,并与运行时 descriptor / runtime entry主键 model保持一致.

V1建议在以下调用点接入该 scope:

- `compiler/graph/fusion/pass/fusion_pass_executor.cc`
- `FusionPassExecutor::InitPassesIfNeed`

其中:

- 首批与后续主链路都只覆盖 `FusionPassExecutor`
- `graph_fusion.cc`不在本方案后续支持范围内

这样可以避免把 Python

### 8.4 Creator和 Context Retrieval Design

当前 `CreateFusionPassFn`是裸函数指针:

- `using CreateFusionPassFn = FusionBasePass *(*)();`

V1不建议把它直接改成 `std::function<FusionBasePass *()>`. 原因有两点:

- Python pass来自 bridge `.so` dynamic registration,如果 creator持有可捕获 lambda,析构链容易和 `dlclose`顺序耦合
- 一旦 `PassRegistry`或其他全局 objects在 bridge `.so`已卸载后才析构, `std::function`内部 object析构就可能访问已卸载 code,存在 `coredump`风险
这里当前不再以 "独立 bridge `.so` `dlclose`风险"作为主原因.更准确原因是:

- 现有 creator ABI仍是无参裸函数指针,直接改成可捕获 objects会扩大影响面
- `std::function`会把运行时 routing information、object析构和调用路径耦合进 creator本身,不利于维持 "creator只做最小 routing、运行时 resources放在 bridge/runtime registry"分层
- 保留裸函数指针 + TLS routing context,仍然是当前影响面最小、最稳妥方案

因此这里建议采用更稳妥方案:

- 保留 `CreateFusionPassFn`为裸函数指针
- 增加一个 "创建期 TLS context"
- Python pass运行时 objects和 metadata放在 bridge持有进程级注册表中

这里说 "标识信息"不是 Python runtime context本身,而是 **用于注册表查找稳定 key和 metadata**,例如:

- `pass_name`
- `pass_kind`,即 `fusion` / `pattern` / `decompose`
- `stage`
- Python模块名
- Python类全名
- `decompose` scenarios下 `op_types`

这些信息属于注册期静态 information,不是执行期 context.真正 Python解释器状态、模块 object、pass实例,不放在 creator里携带,而是放在 bridge全局注册表中统一管理.

推荐实现方式如下:

1. 在消费 `create_fn()`位置设置 TLS创建 context
- 最终建议传入 `descriptor_key`
- 某些现有调用点短期更容易拿到 `pass_name`时,可先做过渡 mapping,但不建议把 `pass_name`固化为最终 creator routing key

2. 为三类 Python pass提供少量通用 creator functions
- `CreatePythonFusionPass()`
- `CreatePythonPatternPass()`
- `CreatePythonDecomposePass()`

3. 通用 creator从 TLS中读取当前 `descriptor_key`
- 再据此从 bridge注册表中找到对应 descriptor
- 然后构造对应 adapter

4. adapter在执行时再从 bridge注册表获取 Python pass实例或其 holder

这个设计优点是:

- 不需要把 Python objects context塞进 `create_fn`
- 不引入 `std::function`析构顺序风险
- 与当前 GE creator调用方式 compatibility更好

### 8.5 TLS Creation Context细化

当前实现需要保留一个轻量创建期 TLS context.它必要性不是 "为了传更多信息",而是因为当前 `FusionPassRegistrationData::CreatePassFn`仍是无参 creator,而 Python pass侧多个 descriptors会共用同一个 adapter factory;如果没有这份 TLS,上层 `PassRegistry::CreatePass()`虽然知道 "正在创建哪个 pass",共享 factory却不知道应该绑定哪个 Python descriptor.

当前 code形态可以表示为:

```cpp
struct PythonPassCreateContext {
  std::string descriptor_key;
  std::string pass_name;
  PythonPassKind kind;
};
```

并提供如下辅助设施:

- `SetCurrentPythonPassCreateContext(descriptor_key)`
- `GetCurrentPythonPassCreateContext()`
- `ClearCurrentPythonPassCreateContext()`
- RAII scope guard,例如 `PythonPassCreateScope`

使用方式如下:

1. 在调用 `create_fn()`前,由调用方设置当前 `descriptor_key`
2. 通用 creator读取 TLS中 `descriptor_key`
3. 通用 creator再到 bridge注册表中查找对应 descriptor
4. 构造对应 adapter
5. `create_fn()`返回后自动清理 TLS

其中:

- `descriptor_key`是真正 routing primary key
- `pass_name` / `kind`当前主要承担一致性校验,避免共享 factory误绑定到错误 descriptor

后续如果调用链进一步统一,仍建议把 `PythonPassCreateContext`收敛到 "只保留 `descriptor_key`这一最小必要 fields",避免状态重复,并与运行时 descriptor / runtime entry主键 model保持一致.

V1建议在以下调用点接入该 scope:

- `compiler/graph/fusion/pass/fusion_pass_executor.cc`
- `FusionPassExecutor::InitPassesIfNeed`

其中:

- 首批与后续主链路都只覆盖 `FusionPassExecutor`
- `graph_fusion.cc`不在本方案后续支持范围内

这样可以避免把 Python化范围扩散到 legacy兼容链路,同时保证 creator / TLS / descriptor方案围绕主链闭环演进.

### 8.6 Bridge Process-level Registry细化

Bridge内部建议维护两个层次注册 information,而不是继续让一个 `holder_key`同时承担 "静态身份"和 "运行时实例"两种 semantics.逻辑上可拆成两部分:

- `PythonPassDescriptor`
- 注册期静态 information

- `PythonPassInstanceHolder`
- 执行期实例 information
- Python pass实例
- 运行期状态
- 异常状态
- session / instance关联 information

其中 `PythonPassDescriptor`建议至少包含:

- `pass_name`
- `pass_kind`
- `stage`
- `module_name`
- `class_qualname`
- `op_types`
- `descriptor_key`

其中:

- `descriptor_key`
- 表示 "这个 pass类是谁"静态 key
- 建议格式为 `module_name + class_qualname + pass_name`
- 用于注册去重、descriptor查找、日志定位

- `instance_id`
- 表示 "这次运行时实例是谁"动态 key
- 由 adapter / session在创建时生成
- 用于 holder查找、实例 lifecycle管理、执行期隔离

最小实现阶段曾把 `holder_key`同时用于 descriptor查找和 holder查找.当前 `FusionBasePass`已完成拆分,后续其余 pass也应保持这套 model:

- `descriptor_key`
- 静态身份

- `instance_id`
- 动态实例身份

V1建议注册表只由 bridge自己持有和析构,不暴露给其他全局 singleton持有 objects,避免再次产生跨 so lifecycle耦合.

这里刻意不把 Python pass实例做成进程级 singleton.原因是:

- User更容易自然地把 `self`当成 "本次 pass执行临时状态容器"
- 可以避免跨图、跨执行残留状态污染
- 可以降低多线程并发下实例共享导致锁和重入要求

### 8.7 Python Pass Adapter细化

V1建议为三类 pass分别提供 adapter:

- `PythonFusionBasePassAdapter`
- `PythonPatternFusionPassAdapter`
- `PythonDecomposePassAdapter`

三者共同特征:

- 构造时只接收 `descriptor_key`或 `pass_name`
- 构造时不直接持有 Python临时 object裸指针
- 构造期完成 descriptor绑定,并为当前 adapter创建独立 `instance_id`
- Adapter lifecycle内独占自己 Python pass实例,不复用长期共享 holder
- 执行时通过 `instance_id`在 bridge实例仓库中查找当前 adapter对应 holder
- 析构时通过 `instance_id`释放 holder
- 执行时统一做 GIL获取、异常转译、状态映射

这样即使 adapter本身由 GE长期持有,它也只依赖 bridge稳定管理 holder,不依赖 creator闭包 object.

三类 adapter分工还需要进一步明确:

- `PythonFusionBasePassAdapter`
- 直接覆盖 `Run()`,内部调用 Python `run(graph, context)`

- `PythonPatternFusionPassAdapter`
- 继承 C++ `PatternFusionPass`
- 不重写基类公共 `Run()`主流程
- 只覆盖 `Patterns()` / `MeetRequirements()` / `Replacement()`三个 hook,并在 hook内部转调 Python

- `PythonDecomposePassAdapter`
- 继承 C++ `DecomposePass`
- 不重写基类公共 `Run()`主流程
- 只覆盖 `MeetRequirements()` / `Replacement()`,并在 hook内部转调 Python

这也是当前设计里 "为什么不急着把 C++ pass基类通过 `PYBIND11_MODULE`暴露给 Python继承"核心原因:真正需要复用 C++非纯虚主流程是 adapter,不是 user写 Python pass classes.

### 8.8 Execution期 Session Design

为避免对 Python pass写法施加不必要限制, V1建议引入 "每次执行一个 session" model:

- 一个 adapter一次 `Run`调用,对应一个 `PythonPassExecutionSession`
- Session内创建新 Python pass实例,并分配唯一 `instance_id`
- 同一 session内,多次 Python回调共用同一个实例

- Session结束时销毁实例,释放 `instance_id`
- Session提供 GIL获取、异常捕获、logging转发等基础封装

这样 user在 Python pass内部可以自由使用 `self`保存临时状态,而不必担心跨执行污染.

### 8.9 Bridge Runtime Registry和 Holder Lookup

当前建议 bridge实现两阶段查找:

1. **Descriptor Lookup** (创建期)
   - 由 adapter构造时根据 TLS `descriptor_key`进行
   - 返回 `PythonPassDescriptor *`

2. **Holder Lookup** (执行期)
   - 由 adapter `Run`内部根据自己 `instance_id`进行
   - 返回 `PythonPassInstanceHolder *`
   - Holder内包含 Python pass实例

查找流程:

```
Adapter构造:
  descriptor_key → GetDescriptor(descriptor_key) → PythonPassDescriptor

Adapter Run:
  instance_id → GetInstanceHolder(instance_id) → PythonPassInstanceHolder → python_pass_instance
```

Bridge registry内部建议使用 concurrent containers (如 `std::unordered_map` + lock或 `tbb::concurrent_hash_map`),以支持多线程创建和执行.

### 8.10 Exception Handling和 Status Mapping

Python pass执行时可能抛出多种异常. V1建议统一转译为 GE status codes:

| Python Exception Type | GE Status Code | Description |
|------|------|------|
| `PassSkipException` | `GRAPH_SUCCESS` (but不 apply changes) | User主动跳过当前 match |
| `PassFatalError` | `GRAPH_FAILED` | User主动标记失败 |
| `PyError` (一般 exception) | `GRAPH_FAILED` + exception message | Python runtime error |
| `GeError` (from GE API calls) | Original GE error code | GE内部 error |

Adapter统一在以下位置捕获:

- `Run()`内部
- `Patterns()`内部
- `MeetRequirements()`内部
- `Replacement()`内部

捕获后统一通过 `context->SetErrorMessage()`记录,并返回对应 status code.

### 8.11 Logging和 Tracing

V1建议 adapter在关键节点添加 tracing:

- Pass creation: `descriptor_key`, `instance_id`
- Pass execution: `graph_id`, `input shapes`, `output shapes`
- Pattern matching: `match_count`, `capture tensors`
- Replacement: `nodes_added`, `nodes_removed`

Tracing可以通过 GE现有 profiling infrastructure输出,或通过 bridge内部 logging facade.

### 8.12 Backward Compatibility和 Versioning

Python pass registry和 adapter设计需要考虑 version compatibility:

- **Descriptor compatibility**: 不同 version pass定义可能存在 optional inputs/attributes差异
- **Adapter compatibility**: Adapter需要处理 legacy Python pass classes
- **Bridge compatibility**: Bridge registry需要支持多 version artifacts共存

V1建议:

- `PythonPassDescriptor`包含 `version` field
- Bridge registry按 `descriptor_key + version`索引
- Adapter根据 descriptor version选择对应 execution strategy

---

## 9 Native Helper和 Code Generation

### 9.1 Native Helper Architecture

Native helper是 bridge内部辅助 module,负责:

- Python object和 C++ object conversion
- Graph structure marshalling
- Tensor data marshalling
- Attribute value marshalling

Native helper位于 bridge `.so`内部,不暴露给 user.

### 9.2 Code Generation Pipeline

Python pass code generation pipeline包含以下 stages:

```
Python Pass Registration
    ↓
Bridge Descriptor Registration
    ↓
Native Helper Binding
    ↓
Adapter Creation
    ↓
GE Pass Registry Entry
```

每个 stage详细说明:

**Stage 1: Python Pass Registration**
- Python user通过 decorator注册 pass
- Python module加载时执行 registration code
- Registration code调用 bridge C API注册 descriptor

**Stage 2: Bridge Descriptor Registration**
- Bridge接收 descriptor information
- Bridge创建 `PythonPassDescriptor` entry
- Bridge分配 `descriptor_key`
- Bridge保存 descriptor到 process-level registry

**Stage 3: Native Helper Binding**
- Bridge根据 descriptor type选择 native helper
- Native helper生成对应 C++ binding code
- Binding code负责 Python和 C++ marshalling

**Stage 4: Adapter Creation**
- GE调用 `CreatePassFn`
- TLS context设置 `descriptor_key`
- Bridge根据 descriptor创建 adapter
- Adapter分配 `instance_id`

**Stage 5: GE Pass Registry Entry**
- Adapter注册到 GE `PassRegistry`
- GE `PassRegistry`保存 adapter pointer
- GE编译流程调用 adapter `Run`

### 9.3 Generated Code Structure

Generated code包含以下 components:

**Header Files**:
- `python_pass_bridge.h`: Bridge public API
- `python_pass_adapter.h`: Adapter definitions
- `python_pass_types.h`: Type definitions

**Source Files**:
- `python_pass_bridge.cc`: Bridge implementation
- `python_pass_adapter.cc`: Adapter implementation
- `python_pass_registry.cc`: Registry implementation

**Generated Files**:
- `python_pass_generated_bindings.cc`: Marshalling bindings
- `python_pass_generated_creators.cc`: Creator functions

### 9.4 Build和 Deployment

Python pass build和 deployment流程:

1. **Native Artifact Generation**
   - C++ code生成 native helper和 adapter
   - 编译为 `_ge_pass_native.so`
   - 编译为 `libge_python_pass_bridge.so`

2. **Python Package Generation**
   - Python code打包为 wheel package
   - Wheel package包含 Python pass modules
   - Wheel package依赖 native artifacts

3. **Deployment**
   - Native artifacts部署到 GE run package
   - Python wheel部署到 Python environment
   - Bridge `.so`加载到 GE process

---

## 10 Runtime Fallback和 Multi-version Support

### 10.1 Runtime Fallback Overview

Runtime fallback是指当 native artifacts不可用时,自动生成 fallback code:

- **Scenario 1**: Python pass requires native helper, but native helper `.so` missing
- **Scenario 2**: Python pass requires specific version artifact, but artifact version mismatch
- **Scenario 3**: Python pass requires platform-specific artifact, but platform not supported

Fallback code generation流程:

```
Detect Missing Native Artifact
    ↓
Generate Fallback Code
    ↓
Compile Fallback Code
    ↓
Load Fallback Artifact
    ↓
Continue Pass Execution
```

### 10.2 Fallback Code Generation

Fallback code generation策略:

**Strategy 1: Pure Python Fallback**
- 纯 Python implementation of pass logic
- 无需 native helper依赖
- 性能较低,但可用性最高

**Strategy 2: Minimal Native Fallback**
- 最小 native helper实现
- 仅支持基本 marshalling
- 性能中等,可用性中等

**Strategy 3: JIT Compilation Fallback**
- 运行时 JIT compile native code
- 支持完整 marshalling
- 性能接近 native,但需要 JIT infrastructure

V1建议采用 Strategy 2,因为:

- Pure Python fallback性能过低
- JIT compilation fallback infrastructure复杂
- Minimal native fallback是当前最优 trade-off

### 10.3 Multi-version Artifact Support

Multi-version artifact是指支持多 Python version artifacts:

- `cp39`: Python 3.9
- `cp310`: Python 3.10
- `cp311`: Python 3.11
- `cp312`: Python 3.12
- `cp313`: Python 3.13
- `cp314`: Python 3.14

每个 version artifact包含:

- Version-specific native helper
- Version-specific adapter
- Version-specific bridge `.so`

Artifact selection流程:

```
Detect Current Python Version
    ↓
Search Version-specific Artifact
    ↓
Load Matching Artifact
    ↓
If Not Found, Generate Fallback
```

### 10.4 Artifact Cache和 Pre-built Artifacts

Pre-built artifacts是指提前编译 native artifacts:

- **Build阶段**: 编译所有 version artifacts
- **Package阶段**: 打包所有 artifacts到 run package
- **Install阶段**: 选择匹配 version artifact安装

Artifact cache是指缓存已生成 fallback artifacts:

- Cache位置: `ge/passes/python_pass_artifacts/`
- Cache key: `python_tag + platform + bridge_abi`
- Cache管理: 自动清理过期 artifacts

---

## 11 Implementation Roadmap

### 11.1 V1 Implementation Scope

V1 implementation scope包含:

**Core Components**:
- `PythonPassDescriptor` registry
- `PythonPassInstanceHolder` management
- `PythonFusionBasePassAdapter`
- `PythonPatternFusionPassAdapter`
- `PythonDecomposePassAdapter`
- TLS creation context
- Bridge registry

**Infrastructure**:
- Native helper binding
- Exception handling
- Logging和 tracing
- Status mapping

**Deployment**:
- Pre-built artifacts generation
- Multi-version support
- Runtime fallback

### 11.2 V1 Implementation Milestones

**Milestone 1: Basic Framework** (Week 1-2)
- Bridge registry implementation
- Descriptor registration API
- TLS creation context
- Basic adapter skeleton

**Milestone 2: Adapter Implementation** (Week 3-4)
- `PythonFusionBasePassAdapter` implementation
- `PythonPatternFusionPassAdapter` implementation
- `PythonDecomposePassAdapter` implementation
- Exception handling

**Milestone 3: Native Helper** (Week 5-6)
- Native helper generation
- Marshalling bindings
- Python-C++ conversion
- Logging和 tracing

**Milestone 4: Multi-version和 Fallback** (Week 7-8)
- Pre-built artifacts generation
- Version-specific artifacts
- Runtime fallback code generation
- Artifact cache management

### 11.3 Future Enhancements

Future enhancements考虑:

**Performance优化**:
- Cache Python pass instances
- Optimize marshalling overhead
- Reduce GIL持有时间

**功能扩展**:
- Support更多 pass types
- Support更多 Python versions
- Support更多

- Session结束时统一释放该实例及其临时包装objects

这意味着:

- `FusionBasePass`
- 一次 `Run`对应一个 Python实例

- `PatternFusionPass`
- 一次 `Run`内部 `Patterns`、`MeetRequirements`、`Replacement`共用同一个 Python实例

- `DecomposePass`
- 一次 `Run`内部对多个匹配节点处理共用同一个 Python实例

这样设计后, Python user可以自然使用:

- `self.xxx`作为一次执行期间临时 cache
- 普通Python objects作为辅助状态
- 普通异常作为失败信号

而无需理解 "这个实例是不是跨图复用"这种 bridge内部细节.

### 8.9 Memory Management细化

V1 memory管理目标是:

- 不要求 Python user手工释放任何 bridge objects
- 不要求 Python user显式使用 `with`、`close()`、`release()`之类 interfaces
- 不允许因为 user把 object存在局部变量或成员变量里就触发双释放或悬挂指针

建议按三层 objects分别处理.

#### 8.9.1 Registration期 Objects

Registration期 objects包括:

- descriptor
- Python模块 object
- Python类 object
- descriptor注册表

这些 objects由 bridge注册表统一持有,桥接层负责引用计数和清理.对 Python user透明.

#### 8.9.2 Execution期 Objects

Execution期 objects包括:

- instance holder
- Python pass实例
- Callback中创建 `Graph` / `Node` / `MatchResult` / `NodeIo` Python包装 objects
- 可能临时 `TensorDesc` / `Shape` / `Tensor`包装 objects
- `instance_id`

这些 objects都绑定到 execution session,而不是绑定到全局 descriptor.

Session结束时:

- Python pass实例释放
- Execution期包装 cache释放
- Execution期有效性 token失效

#### 8.9.3 Borrowed Graph Objects

`Graph`、`Node`、`Tensor`等 objects很多是对 GE当前执行图借用视图.为保证 Python experience不变差,建议:

- Python包装 object内部持有一个 execution期 owner token
- 在 owner token有效时,所有访问都正常工作
- 一旦 user把 object跨 session保存并再次访问,不允许崩溃,而是抛出明确 Python exception,例如:
- `RuntimeError: graph handle has expired`

这样做效果是:

- 不要求文档里给 user增加 "不要缓存这些 objects"硬限制
- 即使 user这么写,也应得到可理解 Python错误,而不是 coredump

#### 8.9.4 TensorDesc / Shape Value Semantics

`TensorDesc`、`Shape`这类 objects建议按 value semantics暴露给 Python:

- Python获得是独立 object
- 可安全保存在局部变量或 `self`上
- 不依赖原始 borrowed graph句柄继续存活

这样更符合 Python user预期,也能减少悬挂引用问题.

#### 8.9.5 Bridge卸载与析构顺序

GE提供两级卸载 semantics,分别对应 "一轮业务结束"和 "进程退出"两个 lifecycle:

##### Unload — 业务级卸载

一轮图编译完成后, GE调用 `UnloadPassPlugins()`清理本轮 pass注册态,但不关闭 Python解释器也不卸载 bridge so.这样下一轮业务可以复用已初始化 Python runtime,避免反复初始化/终结带来开销.

当前实现链路:

```
UnloadPassPlugins()
  → PassPluginLoader::Unload()                              [pass_plugin_loader.cc]
    ├─ UnloadPythonFusionBasePasses()                        // 仅在 python_pass_loaded_为 true时执行
    │   → BridgeLoader::Unload()                             [bridge_loader.cc]
    │     ├─ api_->reset_bridge_state()                      // 通知 bridge清理 Python侧状态并释放 bridge模块引用
    │     ├─ ClearPythonFusionBasePassRuntimeRegistry()      // 清理 C++侧 runtime注册表
    │     └─ PassRegistry::ClearPythonPasses()               // 清理 C++侧 pass注册表
    │   python_pass_loaded_ = false
    └─ CustomPassHelper::Unload()                            // 清理 C++自定义 pass
```

Unload不触及 Python解释器 lifecycle和 bridge so句柄,确保下一轮 `Load()`可以直接复用.

##### ShutdownForProcess — 进程级关闭

进程退出时, GE调用 `ShutdownPassPluginsForProcess()`执行完整资源释放.当前有3个入口可以触发:

- `GEFinalizeV2()` — 在线模式进程结束时
- `aclgrphBuildFinalize()` — 离线编译结束时
- `GeGenerator::Finalize()` — 生成器模式结束时

当前实现链路:

```
ShutdownPassPluginsForProcess()
  → PassPluginLoader::ShutdownForProcess()                   [pass_plugin_loader.cc]
    ├─ 一次性守卫: if (shutdown_done_) return               // 确保进程级 shutdown只执行一次
    ├─ shutdown_done_ = true
    │
    ├─ if (python_pass_loaded_):
    │   UnloadPythonFusionBasePasses()                       // 先清理注册态(同 Unload)
    │     → BridgeLoader::Unload()
    │       ├─ api_->reset_bridge_state()
    │       ├─ ClearPythonFusionBasePassRuntimeRegistry()
    │       └─ PassRegistry::ClearPythonPasses()
    │   python_pass_loaded_ = false
    │
    ├─ ShutdownPythonFusionBasePassesForProcess()            // 无条件执行
    │   → BridgeLoader::ShutdownForProcess()                 [bridge_loader.cc]
    │     ├─ if (api_ != nullptr):
    │     │   api_->shutdown_bridge()                        // 调用 bridge so导出 shutdown
    │     │     → PybindBridge::Shutdown()                   [pybind_bridge.cc]
    │     │       ├─ ResetBridgeStateUnlocked()              // 清理 Python侧状态、释放 bridge模块引用并 gc.collect()
    │     │       └─ if (owns_interpreter_):                 // 仅当解释器由 bridge自己拉起时
    │     │           py::finalize_interpreter()             // 终结 Python解释器
    │     │     owns_interpreter_ = false
    │     ├─ api_ = nullptr                                  // 置空,防止后续再调用
    │     ├─ if (handle_ != nullptr):
    │     │   dlclose(handle_)                               // 卸载 bridge so
    │     │   handle_ = nullptr                              // 置空,防止 dlclose重复
    │     └─ loaded_path_.clear()
    │
    └─ CustomPassHelper::Unload()                            // 清理 C++自定义 pass
```

##### 幂等性保证

由于 `ShutdownPassPluginsForProcess()`可能从多个入口被重复调用,整条链路通过以下守卫保证幂等:

1. **PassPluginLoader层** — `shutdown_done_`标志:首次执行后置为 `true`,后续调用直接返回 `SUCCESS`
2. **BridgeLoader层** — `api_` / `handle_`空指针守卫:首次执行后置为 `nullptr`,后续调用跳过 shutdown和 dlclose
3. **PybindBridge层** — `Py_IsInitialized()`守卫:解释器已终结后不再进入 Python清理逻辑; `owns_interpreter_`守卫确保只终结自己初始化解释器

##### 卸载顺序核心约束

当前实现遵循以下顺序原则:

1. **先清理 C++注册表,再 dlclose bridge so** — `UnloadPythonFusionBasePasses()`先清理 `PassRegistry`和 `PythonFusionBasePassRuntimeRegistry`,之后才执行 `ShutdownForProcess()`进行 `dlclose`.这保证 dlclose时没有任何 C++ object仍在持有 bridge侧回调函数指针.
2. **先清理 Python objects,再终结解释器** — `PybindBridge::Shutdown()`先调用 `ResetBridgeStateUnlocked()`清理 Python侧注册表、holder和动态加载 pass模块,并在 reset内释放 `bridge_module_`引用、调用 `gc.collect()`打破循环引用,最后才调用 `py::finalize_interpreter()`.
3. **先终结解释器,再 dlclose so** — `shutdown_bridge()`在 `BridgeLoader::ShutdownForProcess()`中先于 `dlclose(handle_)`执行,保证 dlclose时 Python解释器已不再运行.
4. **如果解释器已被外部终结** — `Py_IsInitialized()`返回0, bridge跳过所有 Python清理逻辑,仅清理 C++侧状态,不会对已释放 Python objects做 `DECREF`.

这里优先保证 "不崩溃",而不是极限回收所有尾声内存. CPython内部 arena分配器在 `Py_Finalize()`后可能仍有残余内存不被回收,这是 CPython已知行为,不影响进程正常退出.

### 8.10 Lock和 GIL策略细化

Lock和 GIL设计目标是:

- 不把 lock概念暴露给 Python user
- 不要求 Python pass作者自己理解或管理 GIL
- 在 bridge内部把 lock粒度控制到最小,避免把整个 GE pass执行路径串行化

建议分三类 lock.

#### 8.10.1 Bridge Management Lock

用于保护:

- 注册表初始化
- 插件发现
- Holder懒加载
- Unload / finalize状态切换

这类 lock只包围 bridge自己状态管理,不包围 user pass逻辑执行.

#### 8.10.2 Execution Session Lock

每个 execution session可有自己轻量状态保护,但不建议让多个 session共享粗粒度互斥锁.

目标是允许:

- 不同 pass不同执行互不阻塞
- 非 Python纯 C++匹配/改图逻辑继续按原有路径运行

#### 8.10.3 Python GIL

统一规则如下:

- 进入Python前获取 GIL
- 离开Python后立即释放 GIL
- 纯C++图匹配、图遍历、数据整理逻辑不持有 GIL

对三类 pass具体策略:

- `FusionBasePass`
- 回调 `run`时持有 GIL
- Python返回后立刻释放 GIL

- `PatternFusionPass`
- C++ pattern匹配过程不持有 GIL
- 调 `Patterns`、`MeetRequirements`、`Replacement`时短时持有 GIL

- `DecomposePass`
- C++搜索匹配节点时不持有 GIL
- 调 `meet_requirements`、`replacement`时短时持有 GIL

这样可以把 Python执行和 C++执行边界清楚分开,减少不必要全局串行化.

#### 8.10.4 Callback重入策略

V1建议默认支持 "多 session并发、单 session内串行":

- 一个 execution session内部不做并发 Python回调
- 不同 session之间如果由 GE上层并发触发,则通过 GIL自然串行进入 Python

这对 Python user含义是:

- 不需要为了 bridge额外给 pass写 lock
- 如果 user自己使用模块级全局可变状态,仍需自行保证逻辑正确

Bridge不额外限制 user这么写,但也不为 user自己全局共享状态提供自动事务 semantics.

### 8.11 Pythonic Experience约束

V1设计原则是 "把 lifecycle和并发复杂度收敛在 bridge内部",尽量不给 Python user增加非 Pythonic规则.具体要求如下:

- 不要求 user手工管理 memory
- 不要求 user手工管理 lock或 GIL
- 不要求 user通过特定 context manager才能写 pass
- 不要求 user为了避免复用问题而人为拆散普通 Python写法

在可做到范围内, Python user应当能按普通类来写:

- 用构造函数初始化固定配置
- 用 `self`保存一次执行内临时状态
- 用普通 Python异常表示错误
- 用普通返回值表示结果

需要如实说明边界只有两类:

- 注册协议边界
- User仍需通过 decorator或等价注册接口声明 pass,这属于框架接入协议,不属于非 Pythonic限制

- 过期 object边界
- 如果 user跨执行长期保存 borrowed graph视图 object,后续再次访问时会得到 Python异常,而不是被静默支持到无限期

这两类边界是框架接入必需,但不应把 user逼到 "必须按非 Pythonic模式写 code".

### 8.12 `REGISTER_CUSTOM_PASS`后续支持设计

`REGISTER_CUSTOM_PASS`需要支持,但建议放在三类 `PassRegistry` pass稳定之后扩展阶段实施.原因是:

- 其执行路径与 `FusionPassExecutor`体系不同,当前更多走 `CustomPassHelper` / legacy custom pass链路
- 首批优先打通三类 pass,能更快把 descriptor、session、bridge、holder、GIL、异常隔离这些公共底座做稳定
- 等底座稳定后,再接 `REGISTER_CUSTOM_PASS`,可以显著提高代码复用度,避免再做第二套 Python bridge

推荐复用方式如下:

- 继续复用同一套 Python发现机制
  - `ge.passes.bootstrap`
  - 当前阶段以环境变量为主路径,后续再补 `entry_points`

- 继续复用同一套 Python注册表与 descriptor机制
  - 在 `PythonPassKind`中新增 `legacy_custom`
  - Descriptor新增 legacy custom pass所需 metadata

- 继续复用同一套 pybind bridge
  - 不另起第二套 Python runtime初始化
  - 不另起第二套 holder / session管理

### 8.13 PatternFusionPass桥接协议

PatternFusionPass桥接协议定义 adapter如何与 Python pass交互:

#### 8.13.1 Patterns Hook

`Patterns()` hook返回 pattern列表:

- Python实现应返回 `List[Pattern]`或等价结构
- Adapter在 C++侧调用 `Patterns()`,并将返回结果转换为 C++ pattern objects
- GIL只在 `Patterns()`调用期间持有

#### 8.13.2 MeetRequirements Hook

`MeetRequirements(match_result)` hook判断是否满足融合条件:

- Input: `MatchResult` Python wrapper
- Output: `bool` (True/False)
- Adapter在 C++侧调用 `MeetRequirements()`,传入转换后 `MatchResult`
- GIL只在 hook调用期间持有

#### 8.13.3 Replacement Hook

`Replacement(match_result)` hook生成替换图:

- Input: `MatchResult` Python wrapper
- Output: `Graph` Python object
- Adapter在 C++侧调用 `Replacement()`,传入转换后 `MatchResult`
- Adapter将返回 Graph转换为 C++ Graph并执行替换
- GIL只在 hook调用期间持有

---

## 9. Python Graph Interfaces补齐设计

### 9.1 必补能力

Python graph wrapper需要补齐以下能力以支持 Python pass:

- `Graph::FindNodeByName(node_name)`
- `Node::GetAllOutAnchors()`
- `Node::GetAllInAnchors()`
- `Graph::GetParentGraph()`
- `Graph::GetParentNode()`
- Subgraph相关 interfaces

### 9.2 Borrowed Handle

所有 graph wrapper objects都是 borrowed handle:

- 不拥有底层 C++ Graph lifetime
- 依赖 execution session有效性
- Session失效后访问会抛出 Python exception

---

## 10. Packaging和 Release设计

### 10.1 Artifacts

最终产物包含:

**Native Artifacts**:
- `_ge_pass_native.so`: Native helper和 adapter implementations
- `libge_python_pass_bridge.so`: Bridge shared library

**Python Packages**:
- `ge_py_pass_bridge`: Main Python wheel package
- `ge_py_pass_bridge_cp39`: Native sub-wheel for Python 3.9
- `ge_py_pass_bridge_cp310`: Native sub-wheel for Python 3.10
- ... (cp311 to cp314)

**Installation Artifacts**:
- Header files: `python_pass_bridge.h`, `python_pass_adapter.h`
- Runtime libraries: `.so` files
- Python wheels: `.whl` files

### 10.2 Version Strategy

版本策略遵循 Python ABI compatibility:

- Each Python version requires dedicated native artifact
- Native artifacts are backward compatible within same Python minor version
- Python wheel version independent from native artifact version

### 10.3 Installation Strategy

安装策略:

**Run Package Installation**:
- All native artifacts packaged in run package
- Installation script selects matching version artifact
- Installation copies header files and `.so` to target directory

**Python Wheel Installation**:
- Main wheel installed to Python environment
- Native sub-wheel installed based on current Python version
- `pip install --no-index --find-links <path> ge_py_pass_bridge`

### 10.4 Fallback

Fallback机制当 native artifact缺失时:

**Fallback Trigger Conditions**:
1. Native artifact file not found
2. Native artifact version mismatch
3. Native artifact ABI incompatible

**Fallback Code Generation**:
- Generate minimal native helper code
- Generate adapter skeleton code
- Compile generated code to temporary `.so`
- Load temporary artifact
- Cache artifact for reuse

### 10.5 Local验证约束

本地验证需要:

**Environment Requirements**:
- Python interpreter available
- Required Python packages installed
- CANN toolkit installed

**Build Requirements**:
- C++ compiler available
- Required headers and libraries available

**Validation Checklist**:
- Pass registration successful
- Pass execution successful
- Graph modification correct
- Exception handling correct

### 10.6 pybind模块边界

pybind模块边界清晰划分:

**Bridge Module (`pybind_bridge`)**:
- Python解释器管理
- Bridge state management
- Pass module loading
- Exception translation

**Native Helper Module**:
- Graph/Node/Tensor marshalling
- Attribute value marshalling
- Shape/Format conversion

**清晰分离原因**:
- Bridge module依赖 `Python.h`
- Native helper可独立编译和替换
- Avoid circular dependencies

### 10.7 pybind子Wheel组织建议

pybind子wheel组织:

**Directory Structure**:
```
ge_py_pass_bridge/
├── ge/
│   └── passes/
│       ├── __init__.py
│       ├── base.py
│       ├── registry.py
│       └── _bridge.so        # Platform-specific native
└── pyproject.toml

ge_py_pass_bridge_cp39/
├── ge/
│   └── passes/
│       └── native/
│           └── _ge_pass_native.so
└── pyproject.toml
```

**Wheel Metadata**:
- Wheel tag: `cp39-cp39-manylinux2014_x86_64`
- Platform: Linux x86_64
- Python version: 3.9

### 10.8 Fallback Codegen边界

Fallback codegen边界:

**What Gets Generated**:
- Minimal marshalling functions
- Adapter skeleton
- Bridge registration entry

**What NOT Generated**:
- Full native helper (性能 critical)
- Complex optimization code
- Platform-specific optimizations

**Generated Code Location**:
- `ge/passes/python_pass_artifacts/<python_tag>/<platform>/`
- Temporary,可被清理
- Cacheable for reuse

### 10.9 当前工程与后续Codegen兼容策略

兼容策略保证当前工程与后续 codegen平滑演进:

**Phase 1: Pre-built Artifacts**:
- 手动编写 native helper
- 手动编写 adapter
- 手动 build and package

**Phase 2: Codegen Integration**:
- 自动生成部分 marshalling code
- 自动生成 adapter skeleton
- Semi-automated build

**Phase 3: Full Codegen**:
- 自动生成 all native code
- 自动 build and package
- Minimal manual intervention

**Compatibility Guarantee**:
- Phase 1 artifacts compatible with Phase 2
- Phase 2 artifacts compatible with Phase 3
- User Python code unchanged across phases

### 10.10 Python解释器来源与Fallback选择约束

Python解释器来源影响 fallback选择:

**解释器由Bridge初始化**:
- Bridge owns interpreter lifecycle
- Fallback codegen can use current interpreter
- Shutdown时解释器由 bridge终结

**解释器由外部初始化**:
- Bridge does not own interpreter lifecycle
- Fallback codegen limited by external interpreter state
- Shutdown时解释器由外部管理

**Fallback选择约束**:
- Prefer pre-built artifact over fallback
- Use fallback only when necessary
- Fallback codegen limited by interpreter ABI stability

---

## 11. ATC Extension设计

ATC扩展支持 Python pass参数:

**New ATC Parameters**:
- `--py_pass_path`: Python pass plugin path
- `--py_pass_module`: Specific Python module to load

**Parameter Processing**:
- Environment variable `ASCEND_GE_PY_PASS_PATH` primary
- ATC参数补充 specific paths
- Bootstrap discovers all Python passes

**Integration Point**:
- ATC编译前调用 `PassPluginLoader::Load()`
- Pass registration completes before compilation starts
- Compilation uses registered Python passes

---

## 12. File-level Development Plan

### 12.1 Python Package和 Discovery Layer

**Files to Implement**:
- `ge/passes/__init__.py`: Module initialization
- `ge/passes/base.py`: Pass base classes
- `ge/passes/pattern.py`: Pattern matching helpers
- `ge/passes/registry.py`: Pass registry
- `ge/passes/bootstrap.py`: Plugin discovery
- `ge/passes/runtime.py`: Runtime artifact loading

### 12.2 Python Graph Wrapper补齐

**Files to Update**:
- `ge/graph/graph.py`: Add new interfaces
- `ge/graph/node.py`: Add anchor interfaces
- `ge/graph/tensor.py`: Add tensor interfaces

### 12.3 C Wrapper和 Native Bridge

**Files to Implement**:
- `pybind_bridge.cc`: Bridge implementation
- `python_pass_adapter.h`: Adapter definitions
- `python_pass_adapter.cc`: Adapter implementations
- `python_pass_registry.cc`: Registry implementation

### 12.4 Pass注册核心改造

**Files to Modify**:
- `fusion_pass_executor.cc`: Add Python pass loading
- `pass_registry.h`: Add Python pass support
- `pass_registry.cc`: Add Python pass creation

### 12.5 A/B分工与联调边界

**Team A Responsibilities**:
- Python package implementation
- Graph wrapper补齐
- Bootstrap discovery mechanism

**Team B Responsibilities**:
- Bridge implementation
- Adapter implementation
- Registry implementation

**Integration Boundary**:
- Python registration API ↔ Bridge C API
- Bridge ↔ GE Pass Registry

### 12.6 ATC参数接入

**Files to Modify**:
- `atc/main_impl.cc`: Add parameter parsing
- `pass_plugin_loader.cc`: Add ATC integration

### 12.7 Documentation、Samples、Testing

**Documentation**:
- User guide
- API reference
- Design document

**Samples**:
- Simple fusion pass example
- Pattern fusion pass example
- Decompose pass example

**Testing**:
- Unit tests for Python pass
- Integration tests for bridge
- End-to-end compilation tests

---

## 13. Collaboration和推进方式

### 13.1 Collaboration总原则

- Python侧和 C++侧并行开发
- 清晰接口定义优先
- 集成测试及时进行
- 文档同步更新

### 13.2 A/B工作流边界

**Team A**: Python侧开发
- Week 1-2: Package结构搭建
- Week 3-4: Base classes和 registry
- Week 5-6: Bootstrap和 discovery

**Team B**: C++侧开发
- Week 1

### 12.3 C wrapper和 Native Bridge

修改和新增如下 files:

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
- 新增 `_ge_pass_native.so` 源码、导出 header和 build script
- `api/python/ge/ge_api_c_wrapper/CMakeLists.txt`
- `api/python/ge/CMakeLists.txt`
- `compiler/CMakeLists.txt`

职责如下:

- 为 Python graph wrapper提供 C接口
- 提供基于 `pybind11` Python pass bridge / helper so
- 接入 wheel打包与安装

其中建议进一步拆分 responsibility:

- `c_graph.cc` / `c_gnode.cc` / `c_tensor.cc` / `c_match_result.cc`
- 继续服务 `ge.graph` / `ge.es` ctypes路线

- 独立 bridge `.so`
- 负责 Python runtime初始化、descriptor同步、holder管理、adapter原生 logic以及 Python/C++ object转换
- 负责承接预编译 / fallback产物

- `_ge_pass_native.so`
- 负责 `Graph` / `PassContext` / `MatchResult`等 native-backed wrapper与 helper
- 负责与 `base.py` object来源对接
- 不承载 user pass基类,也不要求 user直接 import C++ pass基类继承

- `pass_plugin_loader.cc/.h`
- 负责定位并 `dlopen` bridge `.so`
- 负责和 bridge `.so`稳定 ABI对接

- `python_fusion_base_pass_bridge_c_api.h`
- 定义 bridge loader与 `libge_python_pass_bridge.so`之间稳定 C ABI
- 当前入口为 `GeGetPythonFusionBasePassBridgeApi()`

- `python_fusion_base_pass_bridge_loader.cc`
- 位于 `ge_compiler.so`一侧
- 负责 `dladdr`定位、`dlopen/dlsym`、cache bridge API、以及把 registrar callback传入 bridge
- 当前显式使用 `RTLD_GLOBAL`装载 bridge,以便 embedded CPython后续导入标准库/native extension时能解析到 `libpython` symbols

- `python_fusion_base_pass_pybind_bridge.cc/.h`
- 位于 `libge_python_pass_bridge.so`一侧
- 负责 Python runtime初始化、descriptor同步、holder lifecycle和 `create/run/destroy` callback实现
- 对 `pass_plugin_loader`暴露稳定 ABI,对 Python侧复用 `bootstrap / _bridge`协议

正式架构边界应是 "bridge artifact set可替换, `ge_compiler.so`稳定".其中:

- `libge_python_pass_bridge.so`是 GE内部 loader视角主入口
- `_ge_pass_native.so`是 Python视角 helper extension
- 二者必须作为同一 version、同一 build key配套产物管理
- 当前第一批已把 `python_fusion_base_pass_pybind_bridge.cc`从 `ge_compiler` target中迁出,并新增 `ge_python_pass_bridge` target产出 `libge_python_pass_bridge.so`
- `ge_compiler.so`当前只保留 loader、adapter、registry/runtime entry等稳定 semantics; bridge so才直接依赖 `Python3::Python`与 `pybind_options`

### 12.4 Pass注册核心改造

修改如下 files:

- `compiler/graph/fusion/pass/pass_registry.cc`
- `compiler/graph/fusion/pass/fusion_pass_executor.cc`
- 新增创建期 context管理 files,例如:
- `compiler/graph/fusion/pass/pass_create_context.h`
- `compiler/graph/fusion/pass/pass_create_context.cc`

职责如下:

- 在 `create_fn()`调用点注入 TLS创建 context
- 让通用 creator能按 `pass_name`找到对应 Python descriptor
- 保持现有 C++ pass行为不变

建议职责再细化为:

- `pass_create_context.h/.cc`
- 定义 TLS context与 RAII scope

- `fusion_pass_executor.cc`
- 在 `InitPassesIfNeed`中围绕 `create_fn()`加 scope

- Bridge注册函数
- 将 Python pass descriptor注册为 "固定 creator函数 + pass_name metadata"

说明:

- `graph_fusion.cc`属于 legacy兼容链路,不纳入本方案后续支持范围
- `REGISTER_CUSTOM_PASS`后续支持走独立扩展阶段,但仍复用同一套 descriptor / bridge / session机制

### 12.5 A/B分工与联调边界

当前建议按以下边界并行推进:

- A负责 `libge_python_pass_bridge.so`、`pass_plugin_loader`、`ge_compiler.so`稳定 ABI、adapter路由、fallback装载,以及现有 `ge.graph.Graph` borrowed view接入
- B负责 `_ge_pass_native.so`、`base.py`、`PassContext` / `MatchResult` native-backed wrapper,以及 Python sample / Python API补齐

B需要明确交付:

- `_ge_pass_native.so`构建脚本与模块导出
- `PassContext` borrowed view wrapper
- `MatchResult`最小可用 wrapper
- 必要 helper /工厂 interfaces,供 `libge_python_pass_bridge.so`构造 Python objects
- `base.py` / `pattern.py`中 `PassContext` / `MatchResult` / `Pattern` / `PatternMatcherConfig` native-backed直接导出
- Python sample和 Python API补齐所需最小能力清单

A需要明确交付:

- `graph.py`中 `Graph._create_from(handle, owns_handle, owner)` borrowed / non-owning semantics
- `python_fusion_base_pass_pybind_bridge.cc`中 `BuildPythonGraph()`对现有 `ge.graph.Graph`正式接入
- `libge_python_pass_bridge.so`与 `_ge_pass_native.so`桥接集成点

关于 `Graph`这条边界,需要特别固定如下原则:

- `Graph`优先复用当前已经存在 `ge.graph.Graph`
- `_ge_pass_native.so`不再引入第二套 user可见 `Graph` type
- A负责把 runtime `GraphPtr`以 borrowed view方式接到现有 `ge.graph.Graph`
- B不直接拥有 `Graph` type本身,而是围绕 `PassContext` / `MatchResult` / helper提供配套能力

B完成后, `base.py`应收敛为:

- `FusionBasePass` / `PatternFusionPass` / `DecomposePass`仍然保持纯 Python基类
- `PassContext` / `MatchResult` / `PatternMatcherConfig`直接 re-export `_ge_pass_native`提供 types
- `Pattern`通过 `ge.passes.pattern`直接导出 `_ge_pass_native`提供 types
- 不再为 `_ge_pass_native`缺失场景保留兼容 shim

A在本地拆分 bridge `.so`与 loader时,阶段1可先不依赖 `_ge_pass_native`做最小验证,但这只是一条临时 bring-up策略,不应继续保留到阶段2收口后正式 code中:

- 先继续使用 `FusionBasePass`现有纯 Python合约类
- `_bridge.py`与 bridge `.so`先验证 descriptor、holder、create/run/destroy、`dlopen`、fallback产物选择
- 涉及 `PatternFusionPass`正式端到端验收,等 B `_ge_pass_native`落地后再合流完成

补充约束:

- A不拥有 `base.py`,只消费 B暴露出来稳定 Python interfaces
- B不直接拥有 `libge_python_pass_bridge.so`,只提供 bridge需要使用稳定 Python / native helper能力

### 12.6 ATC参数接入

修改如下 files:

- `api/atc/main_impl.cc`

职责如下:

- 当前不新增新 user pass路由 parameters
- 后续若补 CLI / option,也应最终归一到 `ASCEND_GE_PY_PASS_PATH`或 `ge.passes.bootstrap`统一发现协议

### 12.7 Documentation、Samples、Testing

新增和修改如下 files:

- `examples/fusion_pass/README.md`
- `examples/fusion_pass/python_pass开发指南.md`
- `examples/fusion_pass/*/python/*`
- `tests/ge/ut/ge/graph/pyge_tests/*_test.py`
- `tests/ge/python_pass/*`

职责如下:

- 给出 Python sample
- 给出 user使用 documentation
- 完成单测与集成验证

## 13. Collaboration and Advancement Approach

Notes:

- This design document is responsible for maintaining long-term stable architectural boundaries, A/B collaboration constraints, interface freeze points, and acceptance principles
- `PLAN.md` is the sole source for phase progress, checklists, and completion status
- Phase objectives, A/B sub-objectives, and completed/pending status will only be updated in `PLAN.md`; this design document will no longer maintain similar progress information

### 13.1 Overall Collaboration Principles

It is recommended to continuously advance along two parallel workflows, with the basic principle of "freezing interfaces first, separating write sets as much as possible, and unifying phase status back to `PLAN.md`".

Parallel collaboration follows these constraints:

- Freeze public interfaces first, then develop concurrently:
  - Descriptor/callback protocol between `ge.passes._bridge` and native bridge
  - Minimum interfaces visible to Python in `ge.graph` / `ge.es` / `_ge_pass_native.so`
- Progress tracking is maintained uniformly in `PLAN.md`
- The design document only retains long-term valid boundaries, not process records of "who completed what"
- Phase acceptance can be executed gradually by phase, but whether a "phase is complete" is determined by the checklist in `PLAN.md`

### 13.2 A/B Workflow Boundaries

The long-term boundaries of the two workflows are as follows:

- A focuses on compiler / native bridge / loader / adapter / fallback / existing `ge.graph.Graph` integration
- B focuses on `_ge_pass_native.so`, `base.py`, `PassContext` / `MatchResult`, Python samples, and Python API completion

The fixed boundaries directly related to current implementation are as follows:

- A is responsible for `libge_python_pass_bridge.so`, `pass_plugin_loader`, `ge_compiler.so` stable ABI, and existing `ge.graph.Graph` borrowed view integration
- B is responsible for `_ge_pass_native.so`, `base.py`, native-backed form of `PassContext` / `MatchResult`, and subsequent completion of samples / Python API
- `Graph` prioritizes reusing the existing `ge.graph.Graph`
- `_ge_pass_native.so` no longer introduces a second user-visible `Graph` type
- A does not own `base.py`
- B does not own `libge_python_pass_bridge.so`

### 13.3 Phase Advancement and Delivery Approach

Subsequent phases should collaborate in the following order:

1. First freeze phase boundaries and completion definitions, and write them into `PLAN.md`
2. Then freeze minimum interfaces between A/B
3. Develop in parallel according to write sets
4. Prioritize completing test samples and targeted validation during integration
5. After passing, update checklist status back to `PLAN.md`

For phase definitions themselves, it is recommended to maintain the following long-term order:

- `FusionBasePass` formally connected
- `PatternFusionPass` connected
- `DecomposePass` connected
- Fallback and prebuilt version connected
- Python equivalent implementation of samples
- Supporting documentation, validation, and delivery materials completed

Detailed sub-items, status, and blockers for these phases are uniformly referenced in `PLAN.md` and will not be expanded again in the design document.

### 13.4 Phase Acceptance and Documentation Synchronization

At the conclusion of each phase, it is recommended to complete the following actions synchronously:

- Update the completion status of the corresponding checklist in `PLAN.md`
- If interface boundaries change, update this design document
- If only status changes, do not modify phase descriptions in this design document
- No longer add independent phase progress/acceptance markdown files; process progress is uniformly written back to `PLAN.md`
- Preserve minimum validation commands, result summaries, and known blockers for phase acceptance

This ensures:

- `PLAN.md` reflects real-time progress
- The design document remains stable and is not polluted by process status
- A/B always has a single source of progress during integration

## 14. Validation and Acceptance Requirements

### 14.1 Test Layering

V1 testing is recommended to be organized in four layers:

- Python unit tests:
  - `registry`, `decorator`, `bootstrap`
  - Environment variable primary path, subsequent `entry_points` auto-discovery
  - Descriptor normalization
  - Stale handle checking for borrowed handles
- C++ / native unit tests:
  - Bridge initialization and repeated initialization
  - TLS creation context
  - Dynamic registration
  - Session lifecycle
  - Exception isolation
- Integration tests:
  - `FusionBasePass`
  - `PatternFusionPass`
  - `DecomposePass`
  - `MatchResult`
  - `GeUtils.InferShape`
  - `GeUtils.CheckNodeSupportOnAicore`
- Packaging and installation tests:
  - Main wheel installation
  - Native sub-wheel selection
  - Fallback compilation when no matching version
  - Development path / direct module passing

### 14.2 Phase Acceptance Principles

The completion definition, A/B sub-objectives, status, and blockers for each phase are uniformly based on `PLAN.md`, and this design document will no longer maintain hard-coded acceptance nodes for "phase one/phase two/phase three".

Phase acceptance is recommended to uniformly adopt the following structure:

- Completion definition:
  - Directly reference the "phase completion definition" for the corresponding phase in `PLAN.md`
- Required validations:
  - At least cover one positive main chain
  - At least cover exception paths or failure isolation for newly added interfaces in that phase
  - At least cover lifecycle, repeated loading, or resource release parts directly related to that phase
- Conclusion requirements:
  - `PLAN.md` corresponding checklist status update completed
  - Affected interface boundaries, loading relationships, or responsibility divisions in the design document updated
  - Validation commands, result summaries, and known blockers preserved in phase acceptance records

Additionally, one constraint needs to be followed:

- A capability belongs to whichever phase it is in, and is accepted in that phase, not merged into other phases in advance
- For example, productization capabilities like `entry_points`, prebuilt versions, multi-version native artifacts, and fallback should be based on the corresponding phase in `PLAN.md`, rather than being written into the completion criteria of earlier phases

### 14.3 Milestone Organization Recommendations

Considering the project advances on a "two-week" rhythm, formal milestones are recommended to be organized dynamically according to `PLAN.md` current priorities, rather than hard-coding "certain two phases must be accepted together" in this design document.

It is recommended that each milestone follows these principles:

- One milestone should try to conclude only `1~2` themes that can form a closed loop
- Prioritize organizing around the current main objective, for example:
  - `FusionBasePass` closed loop
  - `PatternFusionPass` closed loop
  - `DecomposePass` closed loop
  - Fallback / prebuilt version closed loop
  - Sample and delivery materials closed loop
- Each milestone should output:
  - Completed checklist delta for this round
  - Targeted validation commands and result summaries
  - Remaining blockers
  - Handoff prerequisites for the next milestone

Extension items are recommended to be organized as separate milestones, not strongly bound to V1 main chain acceptance. For example, `REGISTER_CUSTOM_PASS` is more suitable for separate acceptance after the main chain stabilizes.

### 14.4 Recommended Acceptance Deliverables

Each formal acceptance is recommended to preserve at least the following deliverables:

- Test result summary table:
  - Test case name
  - Covered capability points
  - Execution result
  - Failure reason and conclusion
- Sample execution records:
  - Input model or script
  - Triggered pass
  - Key logs or result summaries
- Known issues list:
  - Whether it blocks the next phase
  - Workaround approach
  - Planned fix phase

### 14.5 Overall Acceptance Dimensions

V1 final acceptance is recommended to be based on the following dimensions:

- Discovery and loading:
  - Discovery mechanisms currently within scope are consistent with `PLAN.md` and consistent with user documentation
  - If `entry_points`, prebuilt versions, or fallback are included in this round, corresponding chains have independent acceptance records
- Three types of pass main chains:
  - `FusionBasePass`, `PatternFusionPass`, `DecomposePass` are discoverable, registrable, creatable, and executable within their respective scopes
- Python / native wrapper:
  - `Graph`, `PassContext`, `MatchResult`, and helpers required by the phase have corresponding capabilities
- Stability:
  - Python pass import failures can be isolated
  - Python pass execution exceptions can be isolated
  - Lifecycle, repeated loading, and resource release semantics have validation records
- Delivery and materials:
  - `PLAN.md`, design documents, samples, validation records, and limitation descriptions remain consistent

### 14.6 Phase Progress Reference

Current phase completion status, A/B sub-objectives, incomplete items, and blockers are not repeatedly maintained in this design document; `PLAN.md` is the sole reference.

This design document only retains the following long-term acceptance requirements:

- Each phase conclusion must synchronously update `PLAN.md`
- If interfaces, responsibility boundaries, or loading relationships change, this design document must be synchronously updated
- If only task status changes, only update `PLAN.md`
- Phase acceptance requires preserving validation commands, result summaries, and known blockers

## 15. Risks and Points of Attention

- If `CreateFusionPassFn` is directly changed to `std::function`, it may introduce `dlclose` and global destructor ordering coupling risks; prioritize adopting the "function pointer + TLS creation context + process-level registry" approach.
- `Graph` borrowed wrapper has been implemented, but subsequently added runtime wrappers must still comply with the constraint of "not taking runtime handle ownership by default" to avoid reintroducing double-free risks.
- Local development environment is Python 3.13, while formal wheel planning is `cp39-cp314`; local testing can only cover Python tags available in the current environment, and the full matrix requires release pipelines to validate separately by Python minor version.
- V1 needs to clearly distinguish two types of native strategies in documentation and implementation: `ge.graph` continues to use ctypes/C wrapper, Python pass bridge uses `pybind11`.
- If more Python version-sensitive logic continues to be compiled directly into `ge_compiler.so`, the evolution space for subsequent bridge artifact sets and fallback will be compressed; therefore, subsequent implementations must converge version differences into replaceable `libge_python_pass_bridge.so` / `_ge_pass_native.so`.
- Within `atc.bin`, Python may be initialized first by TBE, or Python may be initialized first by Python pass bridge; fallback selection must be based on the current in-process interpreter or unified selector, and bridge state cleanup should be designed separately from interpreter finalize, ensuring the interpreter is only finalized by the owner after all Python users have cleaned up.
- `PatternFusionPass` Python implementation is not a simple function callback; it must ensure that the pattern/match/replacement three-stage semantics are consistent with the existing C++ framework.
- Before B's `_ge_pass_native` is implemented, local validation can temporarily not depend on it, but this can only be used for independent bridge `.so` splitting and `FusionBasePass` regression; it cannot replace the final acceptance of `PatternFusionPass`.
- Providing Python equivalents for all 9 samples will expand wrapper coverage; priority should be given to ensuring main chain availability before gradually completing edge interfaces.

## 16. Reuse Boundaries with Subsequent Custom Operator Python Implementation

This scheme is not just for Python pass as a single point service, but is laying a generic foundation for "GE/CANN main framework safely calling Python extension capabilities".

Subsequently, if "custom operator Python implementation" is advanced, a large part of the infrastructure in this scheme can be directly reused, but operator definition, delivery, and sinking-related capabilities still need new dedicated designs.

### 16.1 Directly Reusable Capabilities

The following capabilities can be directly reused in subsequent custom operator Python implementation:

- Python plugin discovery protocol: Currently based on environment variable primary path as baseline, with `entry_points` auto-discovery to be added later; this protocol can be abstracted into a generic Python extension discovery framework.
- Main wheel + native sub-wheel + fallback codegen: This "pure Python main package + multi-Python version native companion package + local fallback compilation when version not matched" release mode is essentially independent of pass/custom op.
- Bridge lifecycle management: Python interpreter initialization, repeated initialization idempotency, exception isolation, unload order control, logging, and diagnostics can all be reused.
- GIL and lock strategies: The strategy of "management lock + session lightweight state + GIL precise surround for Python callbacks" defined in the current document applies to the vast majority of subsequent Python extension integration points.
- Execution session / holder model: Runtime objects and Python wrapper owner tokens, stale handle checks, borrowed objects converting to Python exceptions rather than crashes after becoming invalid; this mechanism also applies to custom operator callback period objects.
- Python registry and descriptor model: The registry, descriptor, bootstrap, and bridge exit in the current pass design can evolve into a generic "Python extension registry".
- Pythonic constraints: User experience goals of not requiring manual object release, not requiring manual GIL management, and not requiring explicit close/release from users should continue to be maintained.

Looking only at the foundation layer of "Python capabilities formally integrated into GE/CANN", the reuse rate at this layer is estimated to reach `60%~70%`.

### 16.2 Partially Reusable Capabilities

The following capabilities can reuse a portion but need to be tailored according to custom operator semantics:

- `ge.graph` / `ge.es` wrapper: If subsequent custom operator Python implementation needs to express schema, infer logic, graph-level replacement, or eager graph entry, these graph wrappers still have value; but they cannot replace operator schema's own modeling.
- Plugin path and installation path management: The current repository already has custom opp path and plugin manager capabilities, such as `custom_op_lib_path_` related logic in `graph_metadef/base/common/plugin/plugin_manager.cc`, which indicates there is an existing foundation for "plugin discovery" and "custom deliverable installation paths"; but Python custom op still needs to define clearer mapping rules between Python packages and OPP packages.
- Error propagation and degradation strategy: The "single plugin failure does not bring down the main chain" approach in current passes still applies, but custom ops are often closer to the compilation main flow than passes; which errors allow degradation and which errors must hard fail needs to be re-graded.
- ATC parameter entry: The parameter integration approach reserved for `--py_pass_path`, `--py_pass_module` in this scheme can be extended to Python custom op later; but parameter naming, priority, and relationship with existing custom opp directory configuration still need to be determined.

This layer is more like "mechanism reuse", not "implementation direct copy". Reuse ratio is roughly `20%~30%`.

### 16.3 Capabilities Needing New Addition for Custom Operators

The following parts cannot be directly obtained from the Python pass scheme and need separate design:

- Custom operator schema/OpDef registration model: Including inputs, outputs, attributes, constraints, default values, version, and namespace.
- Shape/type inference Python registration and execution model.
- Kernel delivery chain: For example, AscendC/Triton/TBE/host-side implementation, compilation artifacts, binary layout, version validation.
- OPP package layout and installation protocol: Directory organization relationships between Python packages, op proto, kernel binaries, tiling files, and configuration files.
- Compile/runtime recognition logic: How ATC and online compilation phases discover, validate, and sink Python custom ops.
- Custom operator and framework adaptor connection: For example, the PyTorch/TensorFlow graph entry methods shown in current `examples/custom_op`, this part is clearly beyond the scope of pass responsibilities.

If the subsequent goal is only "Python writing schema + infer + registration", the reuse from the current pass scheme will be higher; if the goal also includes "Python side fully driving kernel development, packaging, and publishing", the new work will increase significantly.

### 16.4 Reuse Conclusion

It is recommended to view "Python pass implementation" and "Python custom op implementation" as two phases:

- The first phase completes Python pass first, stabilizing the generic foundation of Python plugin integration, lifecycle, memory management, lock/GIL, packaging, and fallback.
- The second phase adds custom operator-specific models on this foundation, including schema, infer, kernel delivery, and OPP layout.

Rough estimate from project dimension:

- Infrastructure layer reuse: `60%~70%`
- Overall project reuse: `40%~50%`

This ratio is sufficient to demonstrate that the current Python pass design is not a one-time solution, but is proactively building a public foundation that subsequent Python custom ops can continuously reuse.
