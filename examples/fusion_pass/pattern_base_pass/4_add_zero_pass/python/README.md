## Python Pass[v1]

该样例同时提供了一个纯 Python 的 `PatternFusionPass` 版本：
[src/test_python_pattern_pass.py](./src/test_python_pattern_pass.py)。
该 sample 复用了本目录的 `Add + 0 -> identity` 语义，演示了以下 Python V1 能力：

- 通过 `PatternMatcherConfigBuilder` 打开常量值匹配
- 使用 `GraphBuilder` 构造 pattern / replacement graph
- 使用 `capture_tensor()` / `MatchResult.get_captured_tensor()` 获取捕获输入
- 通过 `@register_fusion_pass` 以 `PatternFusionPass` 形式注册到 GE

使用方式如下：

1. 设置 Python pass 插件路径
   ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/test_python_pattern_pass.py
   ```
2. 复用[C++ pass样例](../cpp/README.md#程序运行)的 ATC 或在线推理步骤执行模型编译

run包已包含 GE Python 运行时所需的 ge-py wheel，本节不需要再单独安装 `ge_py-0.0.1-py3-none-any.whl`。

预期日志中会出现类似输出：

```text
[PythonAddZeroPass] matched=add_zero_pattern captured=input_0:0
```
