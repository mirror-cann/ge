# ASCEND_GE_PY_PASS_PATH

## Product Support

| Product | Support |
| :----------- | :------: |
| Atlas A3 Training Products/Atlas A3 Inference Products | √ |
| Atlas A2 Training Products/Atlas A2 Inference Products | √ |

## Functionality Description

`ASCEND_GE_PY_PASS_PATH` is GE (Graph Engine) Python Pass system's path discovery environment variable, used to tell GE engine which paths to load user-written Python fusion Pass plugins from.

This environment variable controls a complete "discover-register-execute" chain:

1. When Pass loading flow starts during GE compilation phase, C++ side checks if `ASCEND_GE_PY_PASS_PATH` is set and non-empty.
2. If set, GE synchronizes this environment variable to Python side via C++/Python bridge.
3. Python side scans and loads user-written Pass modules by path.
4. During module loading, users register Pass to Python registry via `@register_fusion_pass` or `@register_decompose_pass` decorators etc.
5. Registration info is passed back to C++ side's PassRegistry, uniformly executed by GE Pass scheduling framework during subsequent compilation phase.

Currently, `ASCEND_GE_PY_PASS_PATH` is **main path** for Python Pass discovery (not fallback mechanism), future versions will supplement `entry_points(group="ge.passes.plugins")` auto-discovery mechanism.

## Value Format

```
ASCEND_GE_PY_PASS_PATH=<path1>[:<path2>[:...]]
```

- Multiple paths separated by colon (`:`).
- Each path can be `.py` file or directory.
- Paths recommend using absolute paths.

### Path Type Description

| Path Type | Scanning Behavior |
| :--- | :--- |
| Single `.py` file | Directly loaded as Python module |
| Directory | Traverse `.py` files not starting with `_` as modules to load; subdirectories containing `__init__.py` imported as Python packages |

## Setting Environment Variable

```bash
# Point to single Python file
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py

# Point to directory (all .py files and Python packages under directory will be scanned)
export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/

# Support multiple paths, separated by colon
export ASCEND_GE_PY_PASS_PATH=/path/to/pass1.py:/path/to/pass_dir2/
```

## Usage Example

For Python Pass development steps, refer to [Python Fusion Pass Development Guide](../../../../../examples/fusion_pass/python_fusion_pass_development_guide.md).
For complete samples, refer to [examples/fusion_pass](../../../../../examples/fusion_pass/README.md) directory, covering development examples for `FusionBasePass`, `PatternFusionPass`, `DecomposePass` etc. Pass types.

## Scanning Rules Detail

### Directory Scanning Rules

When path points to directory, `bootstrap` module scans by following rules:

1. Files and subdirectories starting with `_` under directory will be skipped.
2. `.py` files not starting with underscore loaded as independent modules.
3. Subdirectories containing `__init__.py` imported as Python packages via `importlib.import_module()`.
4. Subdirectories under directory scanned sequentially after sorting by name.

### Module Naming

Each module loaded via file path is assigned internal name in format `_ge_py_pass_{stem}_{hash}`, won't conflict with user module names. Same file path won't be loaded repeatedly.

## Constraints

- Must set this environment variable before GE initialization. GE reads this variable when first loading Pass during compilation phase, dynamically modifying environment variable during runtime won't take effect.
- `.py` files pointed by path must contain valid Python code, and should register at least one Pass using `@register_fusion_pass` or `@register_decompose_pass` decorators. Modules not registering any Pass will be loaded but produce no effect.
- Directory pointed by path must exist, otherwise will throw `FileNotFoundError` during Pass loading phase.
- File paths must have `.py` suffix, otherwise will throw `ValueError`.
- When environment variable value is empty or not set, GE will skip Python Pass loading flow, doesn't affect normal loading of C++ custom Passes.
- Same file path will only be loaded once, repeated appearance in multiple path segments won't cause duplicate registration.