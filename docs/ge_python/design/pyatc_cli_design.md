# pyatc CLI 设计文档

## 1. 背景

ATC 是 GE 离线模型编译工具，现有入口为 `api/atc/atc` shell wrapper。该方式在新进程中启动 ATC，TBE 与自定义 Python pass 使用的 Python 解释器由 ATC 进程自行解析，与用户在终端中选择的 `python` 可能不一致。

本设计在随 CANN 分发的 `ge-py` 中增加 `ge.pyatc`，使用与 ATC 相同的命令行参数，在**当前进程**中进入原有 `ge::main_impl()` 主流程，使 TBE / Python pass 与用户选择的解释器一致。

## 2. 目标与范围

### 2.1 目标

- 提供等价 CLI 入口：`python3 -m ge.pyatc` 与 `<cann_install>/bin/pyatc`。
- ATC 参数、help、stdout / stderr、退出码与原生 ATC 行为一致；不在 Python 层做参数校验或独自维护 help 文案。
- 复用 `ge::main_impl()` 与 `atc_static`，不新增第二套 ATC 主流程。
- OM / OM2 / exeom 产物格式不变。

### 2.2 非目标

- 不在 Python 层重做 ATC 参数解析与编译逻辑。
- 不承诺单进程多次可重入调用、线程安全或并发调用。
- 不将 `main()` 作为稳定对外 Python API。

## 3. 用户体验

使用 CANN 环境前执行 `source <cann_install>/set_env.sh`（使 `pyatc`、`ge` 包与 `libpyatc_wrapper.so` 可被当前解释器加载）。之后：

```bash
pyatc <与 atc 相同的参数>
或 python3 -m ge.pyatc <与 atc 相同的参数>
```

- 命令行参数原样交给 ATC。
- `pyatc --help` 与 `atc --help` 参数内容一致，usage、示例和失败提示中的入口名显示为 `pyatc`。
- `python3 -m ge.pyatc` 是模块入口的推荐写法；用户也可以按环境需要使用其他 Python 3 解释器执行 `-m ge.pyatc`。
- `pyatc` 是安装在 `<cann_install>/bin` 下的 shell wrapper，默认使用当前 `PATH` 解析到的 `python3`，可通过 `PYTHON=/path/to/python pyatc ...` 显式指定解释器。

## 4. 总体架构

- **薄入口**：Python 侧只做参数转发与调用约定适配；ATC 业务逻辑仍在 `main_impl` 与各既有模块中。
- **CANN bin wrapper**：`<cann_install>/bin/pyatc` 只负责补充 CANN `PYTHONPATH` / `LD_LIBRARY_PATH`，并通过 Python 入口函数参数传入 wrapper 绝对路径作为原始 `argv[0]`；不依赖 wheel `console_scripts`，避免绑定安装 wheel 时的 Python 解释器。
- **独立 `libpyatc_wrapper.so`**：链接 `atc_static`，经薄 C 封装进入 `ge::main_impl`；与 `libgraph_wrapper.so`、`libge_runtime_wrapper.so`、`liboffline_compile_wrapper.so` 分离，避免 ATC 全局状态影响其他 ge-py 用法。

## 5. 兼容性

- 原 `atc` / `atc.bin` 与既有 ge-py API 行为不变。
- 旧版本 OM 在新版本下执行不受影响；`pyatc` 为新增入口，依赖新版本 CANN `bin/pyatc` wrapper、ge-py wheel 中的 `libpyatc_wrapper.so` 与模块代码。
- ge-py wheel 不声明 `console_scripts` 入口，避免在 `pip install -t <cann_install>/python/site-packages` 时生成不可被 `PATH` 发现或绑定错误解释器的脚本。

## 6. 验收标准

1. `pyatc --xxx`、`python3 -m ge.pyatc --xxx` 与 `atc --xxx` 输出等价。
2. `pyatc --help`（`<cann_install>/bin/pyatc` shell wrapper）行为与模块入口一致。
3. 典型模型编译参数可透传并产出与 `atc` 一致的 OM。
4. 失败时进程退出码与 ATC 主流程一致。
5. TBE / 自定义 Python pass 路径优先使用当前进程解释器。
