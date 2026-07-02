# ASCEND_GE_PY_PASS_PATH

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 功能说明

`ASCEND_GE_PY_PASS_PATH` 是 GE（Graph Engine）Python Pass 系统的路径发现环境变量，用于告诉 GE 引擎去哪些路径加载用户编写的 Python 融合 Pass 插件。

该环境变量控制的是一个完整的"发现-注册-执行"链路：

1. GE 编译阶段启动 Pass 加载流程时，C++ 侧检查 `ASCEND_GE_PY_PASS_PATH` 是否已设置且非空。
2. 若已设置，GE 通过 C++/Python bridge 将该环境变量同步到 Python 侧。
3. Python 侧按路径扫描并加载用户编写的 Pass 模块。
4. 模块加载过程中，用户通过 `@register_fusion_pass` 或 `@register_decompose_pass` 等装饰器将 Pass 注册到 Python 注册表。
5. 注册信息回传到 C++ 侧的 PassRegistry，在后续编译阶段由 GE Pass 调度框架统一执行。

当前阶段，`ASCEND_GE_PY_PASS_PATH` 是 Python Pass 发现的**主路径**（而非兜底机制），后续版本将补充 `entry_points(group="ge.passes.plugins")` 自动发现机制。

## 取值格式

```
ASCEND_GE_PY_PASS_PATH=<path1>[:<path2>[:...]]
```

- 多个路径之间以冒号（`:`）分隔。
- 每个路径可以是 `.py` 文件或目录。
- 路径建议使用绝对路径。

### 路径类型说明

| 路径类型 | 扫描行为 |
| :--- | :--- |
| 单个 `.py` 文件 | 直接作为 Python 模块加载 |
| 目录 | 遍历其中不以 `_` 开头的 `.py` 文件作为模块加载；包含 `__init__.py` 的子目录作为 Python 包导入 |

## 设置环境变量

```bash
# 指向单个 Python 文件
export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py

# 指向目录（目录下所有 .py 文件和 Python 包都会被扫描）
export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/

# 支持多个路径，以冒号分隔
export ASCEND_GE_PY_PASS_PATH=/path/to/pass1.py:/path/to/pass_dir2/
```

## 使用示例

Python Pass 开发步骤请参考 [Python 融合 Pass 开发指南](../../../../../examples/fusion_pass/python_fusion_pass_development_guide.md)。
完整样例请参考 [examples/fusion_pass](../../../../../examples/fusion_pass/README.md) 目录，涵盖
`FusionBasePass`、`PatternFusionPass`、`DecomposePass` 等 Pass 的开发示例。

## 扫描规则详解

### 目录扫描规则

当路径指向目录时，`bootstrap` 模块按以下规则扫描：

1. 目录下以 `_` 开头的文件和子目录将被跳过。
2. 非下划线开头的 `.py` 文件作为独立模块加载。
3. 包含 `__init__.py` 的子目录作为 Python 包通过 `importlib.import_module()` 导入。
4. 目录下的子目录按名称排序后依次扫描。

### 模块命名

每个通过文件路径加载的模块会被赋予一个内部名称，格式为 `_ge_py_pass_{stem}_{hash}`，不会与用户模块名称冲突。同一个文件路径不会被重复加载。

## 约束说明

- 必须在 GE 初始化之前设置该环境变量。GE 在编译阶段首次加载 Pass 时读取该变量，运行过程中动态修改环境变量不会生效。
- 路径指向的 `.py` 文件必须包含合法的 Python 代码，且应使用 `@register_fusion_pass` 或 `@register_decompose_pass` 装饰器注册至少一个 Pass。未注册任何 Pass 的模块会被加载但不会产生效果。
- 路径指向的目录必须存在，否则会在 Pass 加载阶段抛出 `FileNotFoundError`。
- 文件路径必须以 `.py` 为后缀，否则会抛出 `ValueError`。
- 环境变量值为空或不设置时，GE 将跳过 Python Pass 加载流程，不影响 C++ 自定义 Pass 的正常加载。
- 同一个文件路径只会被加载一次，重复出现在多个路径段中不会导致重复注册。
