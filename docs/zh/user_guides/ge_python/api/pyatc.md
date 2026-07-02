# pyatc

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```bash
# 命令行方式
python -m ge.pyatc [ATC 参数...]

# Python 代码方式
from ge.pyatc import main
```

## 功能说明

`pyatc` 模块提供与 `atc`（Ascend Tensor Compiler）命令行工具等价的 Python 入口，便于用户在指定 Python 解释器的进程内执行 ATC 编译命令。

该模块的核心用途是解决 ATC 进程内需要使用特定 Python 解释器的场景，例如 Python Pass 插件的加载和执行。通过 `pyatc`，用户可以确保 ATC 使用当前 Python 环境而非系统默认 Python。

## 函数原型

### main

```python
def main(cmdline_argv0=None, args=None) -> None
```

ATC 命令行主入口函数。将参数编码后调用底层 C++ ATC 主函数执行编译，以返回值作为退出码退出进程。

## 参数说明

### main

| 参数 | 类型 | 必选 | 说明 |
| :--- | :--- | :---: | :--- |
| cmdline_argv0 | Optional[str] | 否 | 自定义 argv[0]，即命令行程序名称。若为 None，则使用 `sys.argv[0]`（当 argv[0] 为 `__main__.py` 时自动替换为 `sys.executable + " -m ge.pyatc"`） |
| args | Optional[List[str]] | 否 | ATC 命令行参数列表。若为 None，则使用 `sys.argv[1:]` |

## 返回值说明

`main` 函数无返回值。执行完成后以底层 ATC 返回码调用 `sys.exit()` 退出进程。

## 约束说明

- `main` 函数会调用 `sys.exit()` 终止进程，不适合在需要继续执行后续逻辑的场景中使用。
- ATC 命令行参数与 `atc` 命令行工具一致，具体参数说明请参考 ATC 相关文档。
- 该模块需要 CANN 环境已正确安装和配置。

## 使用示例

```bash
# 命令行方式，等价于 atc 命令（仅示例, atc命令完整合法性不做保证）
python -m ge.pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
# 或者
pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model


# 带 Pass 路径的场景（仅示例, atc命令完整合法性不做保证）
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
python -m ge.pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
# 或者
pyatc --model=model.onnx --soc_version=Ascend910B1 --output=model
```
