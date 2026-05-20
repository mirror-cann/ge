# MoveReluBeforeConcatPass Python 样例使用指导

本目录提供 `graph_base_pass/2_move_relu_before_concat_pass` 的 **纯 Python** 版本示例，逻辑与 C++ 版本一致：

- 扫描 `ConcatV2 -> Relu` 结构
- 构造 replacement 子图（将 Relu 前移到 Concat 的每个输入上）
- 使用 `SubgraphBoundary` + `SubgraphRewriter.replace()` 做子图替换

> **FusionBasePass与PatternFusionPass**
> 
> 本样例中pass继承FusionBasePass，与继承PatternFusionPass不同，这里通过重写Run函数实现pass逻辑。
> Run函数入参包含graph对象的引用，本样例场景中Concat节点输入数量不固定，
> 难以用固定的pattern表示，而在Run函数里可以根据目标图中匹配到的Concat节点动态构造Boundary，实现更高的灵活性。

## 前置条件

- 已 source CANN 环境（`source ${ASCEND_PATH}/set_env.sh`）
- Python 环境可导入 ES API（通常来自 run 包/ops 包）：
  - `ge.es.math.ConcatV2`
  - `ge.es.nn.Relu`（若环境中没有该符号，请确认 ES Python API 安装完整）
- 可导入 GE Python 包（含 `ge.graph`、`ge.passes` 及 pass 加载链路）

## 使用方式

1. 通过环境变量让 GE 在编译期加载该 Python pass：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_move_relu_before_concat_pass.py
```

2. 复用 [C++ pass 样例 README](../cpp/README.md) 中的程序编译+程序执行进行验证。

## 预期现象

日志中会出现类似打印：

```text
PythonMoveReluBeforeConcatPass
Replacement of PythonMoveReluBeforeConcatPass succeeded
```

