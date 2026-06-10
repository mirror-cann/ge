# ConvTransFormatPass Python 样例使用指导

本目录提供 `graph_base_pass/3_modify_conv_data_format_pass` 的 **纯 Python** 版本示例，主链路与 C++ `ConvTransFormatPass` 一致：

- 遍历图中 `Conv2D` / `Conv2DV2`，筛选 `data_format == NCHW` 的节点；
- 将 `data_format` 改为 `NHWC`；
- 从卷积输出做 BFS，按顺序匹配 `perm == [0,2,3,1]` 与 `[0,3,1,2]` 的 `Transpose`，删除对应 `Transpose` 与 perm 常量产点并重连数据边。

本样例继承 `FusionBasePass` 并重写 `run()`，通过 `Graph.remove_edge` / `add_data_edge` / `remove_node` 与 `Node.set_attr` 完成改写，**不使用** `SubgraphRewriter`。

## 与 C++ 版本的差异

1. **整图回滚**：C++ 在 `SetAttr` 或删边失败时用备份图恢复。当前 Python `Graph` 不支持整图深拷贝，本样例在失败时抛出异常，**不保证**与 C++ 相同的原子回滚语义。
2. **读取 Transpose 的 perm**：C++ 使用 `GNode::GetInputConstData`。Python 侧通过 perm 输入端的 `Const` / `Constant` 节点的 `value` 属性读取（与 `pattern_base_pass/4_add_zero_pass` 中 Const 校验方式一致）。若图中 perm 不以此形式出现，可能无法识别并删除 `Transpose`，与 C++ 覆盖范围可能略有差别。

## 目录结构

```
python/
├── README.md                     // Python 样例说明
├── CMakeLists.txt                // 生成 es_all Python ES API 的编译脚本
├── src
│   ├── python_modify_conv_data_format_pass.py // Python pass 实现文件
```

## 前置条件

- 已完成 CANN 环境变量设置，设置方式为 `source ${ASCEND_PATH}/set_env.sh`，更多指导请参考 [C++ 样例 README](../cpp/README.md) 的配置环境变量步骤
- 可导入 GE Python 包（含 `ge.graph`、`ge.passes` 及 pass 加载链路）

Python pass 运行时会加载基于 `pybind11` 的预编译二进制组件。CANN 包优先提供与当前 Python 版本匹配的产物；若无匹配产物，会自动进入 fallback 编译流程。fallback 编译需要当前 Python 环境中已安装 `pybind11`。

## 使用方式

1. 通过环境变量让 GE 在编译期加载该 Python pass（在 `3_modify_conv_data_format_pass` 目录下时）：

```bash
export ASCEND_GE_PY_PASS_PATH=$PWD/python/src/python_modify_conv_data_format_pass.py
```

2. 参考 [C++ 样例 README](../cpp/README.md) 的程序运行章节进行验证。

## 预期现象

日志中会出现类似打印：

```text
PythonConvTransFormatPass is starting
Remove output edges success
Remove output edges success
PythonConvTransFormatPass completed
```

对比 `DUMP_GE_GRAPH` 导出的 pbtxt 时，应看到卷积 `data_format` 为 `NHWC`，且目标 `Transpose` 被移除（与 C++ 样例说明一致）。
