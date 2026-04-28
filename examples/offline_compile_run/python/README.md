# 样例使用指导

## 1、功能描述
本样例演示离线图编译执行的流程，关于编译图为离线模型更多信息，请参考 [生成离线模型](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/graph/graphdevg/atlasag_25_0030.html)。

## 2、目录结构
```angular2html
python/
├── src/
|   ├── single_model/               // 单模型样例
|   |   ├── build_add_model.py      // 离线编译 Add 图，生成 add_sample.om
|   |   └── run_add_model.py        // 加载 add_sample.om 并推理
|   ├── bundle_model/               // Bundle 模型样例
|   |   ├── build_bundle_model.py   // 将 Add/Mul 多图打成一包，生成 bundle_sample.om
|   |   └── run_bundle_model.py     // 加载 bundle_sample.om，依次执行各子模型
|   └── common.py                   // 公共逻辑
├── README.md                       // README 文件
├── run_sample.sh                   // 执行脚本
```

## 3、使用方法
### 3.1、准备cann包
- 本样例需要同时安装两套 CANN：最新开发包用于图编译，官网正式发布包提供 [`pyACL`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/appdevg/acldevg/aclpythondevg_0096.html) 模块用于图执行。本文中的“编译”和“执行”均特指图编译和图执行，不指 GE 工程源码编译。
- 安装请参考：
  - 最新开发包，用于图编译，提供本样例依赖的最新 GE/Python 能力。安装请参考 [安装指导](../../../docs/quick_install.md) 中“方式三：手动安装软件包 > 场景1：体验master版本能力或基于master版本进行开发”，安装最新版本的 `toolkit` 和 `ops` 包
  - 官网正式发布的 CANN `toolkit` 和 `ops` 包，用于图执行，提供 `pyACL`。安装请参考 [安装指导](../../../docs/quick_install.md) 中“方式三：手动安装软件包 > 场景2：体验已发布版本能力或基于已发布版本进行开发”，安装官网正式发布版本的软件包
- 设置环境变量 (假设最新开发包安装在/usr/local/Ascend/，官网正式发布包安装在/usr/local/Ascend-release/)
```
source /usr/local/Ascend/cann/set_env.sh
export PYTHONPATH="$PYTHONPATH:/usr/local/Ascend-release/cann/python/site-packages"
```

### 3.2、图编译和图执行

执行单模型样例：
```bash
bash run_sample.sh -t sample_and_run_python
```
该命令会：
1. 构建 `Add` 图，离线编译并生成 `add_sample.om`
2. 通过 pyACL 加载并执行该离线模型

执行 bundle 样例：
```bash
bash run_sample.sh -t sample_and_run_bundle_python
```
该命令会：
1. 将 `Add` 图与 `Mul` 图打成 Bundle，离线编译并生成 `bundle_sample.om`
2. 通过 pyACL 加载 Bundle，并分别执行两个子模型

离线编译在无卡场景下如需指定目标芯片版本，可增加 `--soc-version`：
```bash
bash run_sample.sh --soc-version Ascend910B1 -t sample_and_run_python
bash run_sample.sh --soc-version Ascend910B1 -t sample_and_run_bundle_python
```

也可以拆分为“只做图编译”和“只做图执行”两个阶段：
```bash
bash run_sample.sh -t build_model
bash run_sample.sh -t run_infer

bash run_sample.sh -t build_bundle_model
bash run_sample.sh -t run_bundle_infer
```

执行成功后会看到：
```text
[Success] sample 执行成功
```

#### 输出文件说明

执行成功后会在当前目录生成以下文件：
- `add_sample.om` - 单模型离线文件
- `bundle_sample.om` - Bundle 离线模型文件

### 3.3、日志打印
可执行程序执行过程中如果需要日志打印来辅助定位，可以在 `bash run_sample.sh` 之前设置如下环境变量来让日志打印到屏幕
```bash
export ASCEND_SLOG_PRINT_TO_STDOUT=1 #日志打印到屏幕
export ASCEND_GLOBAL_LOG_LEVEL=0 #日志级别为debug级别
```

## 4、核心流程介绍

### 4.1、单模型离线编译执行
- 使用 `build_initialize` 初始化编译环境
- 构建 `Graph` 并通过 `build_model` 生成离线模型
- 使用 `save_model` 保存 `om` 文件
- 通过 `acl.mdl.load_from_file`、`acl.mdl.execute` 执行离线模型

### 4.2、Bundle 离线编译执行
- 使用 `GraphWithOptions` 组织多个 `Graph`
- 通过 `bundle_build_model` 一次性构建 Bundle 模型
- 使用 `bundle_save_model` 保存 `bundle_sample.om`
- 通过 `acl.mdl.bundle_load_from_file` 加载 Bundle，并逐个执行子模型
