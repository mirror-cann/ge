# ATC Raw GE Options

## 1. 概述

ATC 离线编译入口支持两类 GE compile option 输入：

- 显式命令行参数，例如 `--jit_compile`、`--optimization_switch`、`--static_model_ops_lower_limit`
- `--raw_ge_options=<json_path>` 指向的 JSON 文件

ATC 仍然只向 GE Compiler 传递一份扁平的 `std::map<std::string, std::string>` options。raw JSON 只改变 options 的来源和合并顺序，不改变 GE Compiler、Runtime、OM 序列化格式或 global/session/graph option API。

## 2. raw_ge_options 格式

ATC 只解析顶层 `"compile options"`，并忽略 `"execute options"`：

```json
{
  "compile options": {
    "global": {
      "ge.jit_compile": "1"
    },
    "session": {
      "ge.jit_compile": "1"
    },
    "graph": {
      "ge.jit_compile": "2"
    }
  }
}
```

`"compile options"` 下只允许 `global`、`session`、`graph` 三个对象。三段配置会被压平到同一份 options map，覆盖顺序为：

```text
graph > session > global
```

每个 option key 必须非空，每个 option value 必须为 JSON string。

## 3. 支持性与优先级

raw option 支持性以 ATC 已支持的 GE option 为准：

- `api/atc/atc_option_map.cc` 中 `BuildAtcGeOptionToCliNameMap()` 维护的 GE option key
- `OptionRegistry::GetVisibleOptions(OoEntryPoint::kAtc)` 注册的 ATC 可见优化项

`--raw_ge_options_ignore_unsupported=false` 时，raw 文件中出现不支持 option 会失败；为 `true` 时，不支持项会被跳过，其他支持项继续生效。

最终合并规则为：

1. ATC 先解析命令行，并记录用户显式传入的 CLI 参数
2. raw 文件解析并校验得到 raw options
3. 若 raw key 对应 ATC 必填 CLI 参数，且该 CLI 参数未被用户显式设置，则报错，raw 不能替代必填 CLI
4. 若 raw key 对应的 ATC CLI 参数未被用户显式设置，则将 raw value 写入对应 `FLAGS_*`
5. 后续 `CheckFlags()`、`ParseGraph()`、`SetOutputNodeInfo()` 和 options 构造继续走原 CLI 路径
6. 对 `OptionRegistry` 注册的动态优化项，raw 也会按 CLI 显式优先规则合入最终 options
7. `AppendOptimizationOptions()` 最后追加 ATC 固定优化开关，例如 `forbidden_close_pass:on`

该顺序保证 `ge.outputDatatype`、`ge.inputShape`、`ge.enableCompressWeight`、`ge.status_check` 等依赖 ATC 前置解析、值转换或副作用的 option 与 CLI 行为一致。

## 4. 约束

- raw 文件不能替代 ATC 必填参数，例如 `--model`、`--framework`、`--soc_version`、`--output`
- raw 文件中的 option value 先复用对应 CLI flag 的基础类型和枚举校验；例如 `ge.jit_compile` 必须为 `"0"`、`"1"` 或 `"2"`
- `ge.exec.static_model_ops_lower_limit` 额外复用 CLI 的 `[-1, INT64_MAX]` 范围校验
- 涉及多个 option 的组合约束在 raw 注入 `FLAGS_*` 后由原 `CheckFlags()` 统一校验，避免 raw 与显式 CLI 组合时出现双套规则
- OM2 模式下，raw 注入的 flag 会和用户显式 CLI 参数一起参与 OM2 不支持参数检查
- `ge.optimizationSwitch` 在 ATC 入口按字符串透传，后续由现有 OptimizationOption 流程解析；动态注册优化项复用其注册的 value checker
- 本特性仅适用于 ATC 离线编译入口，不影响在线 Session/API 入口
