# ATC Raw GE Options

## 1. Overview

ATC offline compilation entry supports two types of GE compile option inputs:

- Explicit command-line parameters, e.g., `--jit_compile`, `--optimization_switch`, `--static_model_ops_lower_limit`
- JSON file pointed to by `--raw_ge_options=<json_path>`

ATC still only passes one flattened `std::map<std::string, std::string>` options to GE Compiler. raw JSON only changes the source and merge order of options, not changing GE Compiler, Runtime, OM serialization format or global/session/graph option API.

## 2. raw_ge_options Format

ATC only parses top-level `"compile options"` and ignores `"execute options"`:

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

Only `global`, `session`, `graph` three objects are allowed under `"compile options"`. Three segments of configuration will be flattened to same options map, with override order:

```text
graph > session > global
```

Each option key must be non-empty, each option value must be JSON string.

## 3. Support and Priority

raw option support is based on GE options already supported by ATC:

- GE option keys maintained in `BuildAtcGeOptionToCliNameMap()` in `api/atc/atc_option_map.cc`
- ATC visible optimization items registered in `OptionRegistry::GetVisibleOptions(OoEntryPoint::kAtc)`

When `--raw_ge_options_ignore_unsupported=false`, unsupported options appearing in raw file will fail; when `true`, unsupported items will be skipped, other supported items continue to take effect.

Final merge rules:

1. ATC first parses command line, recording user explicitly passed CLI parameters
2. raw file is parsed and validated to get raw options
3. If raw key corresponds to ATC required CLI parameter, and that CLI parameter was not explicitly set by user, then error, raw cannot replace required CLI
4. If raw key's corresponding ATC CLI parameter was not explicitly set by user, then write raw value to corresponding `FLAGS_*`
5. Subsequent `CheckFlags()`, `ParseGraph()`, `SetOutputNodeInfo()` and options construction continue on original CLI path
6. For dynamically registered optimization items in `OptionRegistry`, raw also merges into final options by CLI explicit priority rule
7. `AppendOptimizationOptions()` finally appends ATC fixed optimization switches, e.g., `forbidden_close_pass:on`

This order guarantees `ge.outputDatatype`, `ge.inputShape`, `ge.enableCompressWeight`, `ge.status_check` and other options depending on ATC pre-parsing, value conversion or side effects behave consistently with CLI.

## 4. Constraints

- raw file cannot replace ATC required parameters, e.g., `--model`, `--framework`, `--soc_version`, `--output`
- Option values in raw file first reuse corresponding CLI flag's basic type and enum validation; e.g., `ge.jit_compile` must be `"0"`, `"1"` or `"2"`
- `ge.exec.static_model_ops_lower_limit` additionally reuses CLI's `[-1, INT64_MAX]` range validation
- Combination constraints involving multiple options are uniformly validated by original `CheckFlags()` after raw injection into `FLAGS_*`, avoiding dual rules when raw combines with explicit CLI
- In OM2 mode, raw injected flags participate in OM2 unsupported parameter check together with user explicit CLI parameters
- `ge.optimizationSwitch` is transparently passed as string at ATC entry, subsequently parsed by existing OptimizationOption flow; dynamically registered optimization items reuse their registered value checker
- This feature only applies to ATC offline compilation entry, does not affect online Session/API entry
