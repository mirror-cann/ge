# ASCEND\_GE\_PY\_PASS\_PATH

## 功能描述

ASCEND\_GE\_PY\_PASS\_PATH是 GE（Graph Engine）Python Pass系统的路径发现环境变量，用于告诉GE引擎去哪些路径加载用户编写的Python融合Pass插件。

该环境变量控制的是一个完整的“发现-注册-执行”链路：

1. GE编译阶段启动Pass加载流程时，C++侧检查ASCEND\_GE\_PY\_PASS\_PATH是否已设置且非空。
2. 若已设置，GE通过C++/Python bridge将该环境变量同步到Python侧。
3. Python侧按路径扫描并加载用户编写的Pass模块。
4. 模块加载过程中，用户通过@register\_fusion\_pass或@register\_decompose\_pass等装饰器将Pass注册到Python注册表。
5. 注册信息回传到C++侧的PassRegistry，在后续编译阶段由GE Pass调度框架统一执行。

当前阶段，ASCEND\_GE\_PY\_PASS\_PATH是Python Pass发现的**主路径**（而非兜底机制），后续版本将补充entry\_points\(group="ge.passes.plugins"\)自动发现机制。

配置取值：

```bash
ASCEND_GE_PY_PASS_PATH=<path1>[:<path2>[:...]]
```

路径要求：多个路径之间以冒号（:）分隔；每个路径可以是.py文件或目录；路径建议使用绝对路径。

路径类型说明：

| 路径类型 | 扫描行为 |
| --- | --- |
| 单个.py 文件 | 直接作为Python模块加载 |
| 目录 | 遍历其中不以_开头的.py文件作为模块加载；包含__init__.py的子目录作为Python包导入 |

## 配置示例

- 指向单个Python 文件

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
    ```

- 指向目录（目录下所有 .py 文件和 Python 包都会被扫描）

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/
    ```

- 支持多个路径，以冒号分隔

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/pass1.py:/path/to/pass_dir2/
    ```

## 扫描规则

- **目录扫描规则**

    当路径指向目录时，bootstrap模块按以下规则扫描：

    1. 目录下以\_开头的文件和子目录将被跳过，不参与扫描。
    2. 非下划线开头的.py文件作为独立模块直接加载。
    3. 包含\_\_init\_\_.py的子目录作为Python包通过importlib.import\_module\(\)导入。
    4. 目录下的子目录按名称排序后依次扫描。

- **模块命名**

    每个通过文件路径加载的模块均分配唯一内部名称，格式为\_ge\_py\_pass\_\{stem\}\_\{hash\}，避免与用户自定义模块名冲突。同一个文件路径不会被重复加载。

## 使用约束

- 必须在GE初始化之前设置该环境变量。GE在编译阶段首次加载Pass时读取该变量，运行过程中动态修改环境变量不会生效。
- 路径指向的.py 文件必须包含合法的Python代码，且应使用@register\_fusion\_pass或@register\_decompose\_pass装饰器注册至少一个Pass。未注册任何Pass的模块会被加载但不会产生效果。
- 路径指向的目录必须存在，否则会在Pass加载阶段抛出FileNotFoundError。
- 文件路径必须以.py为后缀，否则会抛出ValueError。
- 环境变量值为空或不设置时，GE将跳过Python Pass加载流程，不影响C++自定义Pass的正常加载。
- 同一个文件路径只会被加载一次，重复出现在多个路径段中不会导致重复注册。

## 支持的型号

全量芯片支持
