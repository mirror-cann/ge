# 样例使用指导

## 1、功能描述

本样例演示离线图编译执行的流程，关于编译图为离线模型更多信息，请参考 [生成离线模型](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta1/graph/graphdevg/atlasag_25_0030.html)。

## 2、目录结构

```text
cpp/
├── CMakeLists.txt           // CMake 构建文件
├── main.cpp                 // 程序主入口
├── run_sample.sh            // 执行脚本
├── README.md                // README 文件
└── src/
    ├── CMakeLists.txt            // CMake 构建文件
    ├── common.h / common.cpp     // 公共逻辑文件
    ├── single_model/             // 单模型编译与推理
    └── bundle_model/             // Bundle 编译与推理
```

## 3、使用方法

### 3.1、准备cann包
- 通过 [安装指导](../../../docs/quick_install.md) 中“方式三：手动安装软件包 > 场景1：体验master版本能力或基于master版本进行开发”，正确安装`toolkit`和`ops`包
- 设置环境变量 (假设包安装在/usr/local/Ascend/)
```
source /usr/local/Ascend/cann/set_env.sh 
```

### 3.2、图编译和图执行

执行单模型样例：
```bash
bash run_sample.sh -t sample_and_run
```
该命令会：
1. 编译 C++ 可执行程序
2. 构建 `Add` 图，离线编译并生成 `add_sample.om`
3. 加载、执行该离线模型

执行 bundle 样例：
```bash
bash run_sample.sh -t sample_and_run_bundle
```
该命令会：
1. 编译 C++ 可执行程序
2. 将 `Add` 图与 `Mul` 图打成 Bundle，离线编译并生成 `bundle_sample.om`
3. 加载 Bundle，并分别执行两个子模型

离线编译在无卡场景下如需指定目标芯片版本，可增加 `--soc-version`：
```bash
bash run_sample.sh -s Ascend910B1 -t sample_and_run
bash run_sample.sh -s Ascend910B1 -t sample_and_run_bundle
```

也可以拆分为"只做图编译"和"只做图执行"两个阶段：
```bash
bash run_sample.sh -t build_model
bash run_sample.sh -t run_infer

bash run_sample.sh -t build_bundle_model
bash run_sample.sh -t run_bundle_infer
```
> `run_infer` / `run_bundle_infer` 要求 om 模型已存在

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
- 使用 `aclgrphBuildInitialize` 初始化编译环境
- 构建 `Graph` 并通过 `aclgrphBuildModel` 生成离线模型
- 使用 `aclgrphSaveModel` 保存 `om` 文件
- 通过 `aclmdlLoadFromFile`、`aclmdlExecute` 执行离线模型

### 4.2、Bundle 离线编译执行
- 使用多个 `Graph` 组织 Bundle
- 通过 `aclgrphBundleBuildModel` 一次性构建 Bundle 模型
- 使用 `aclgrphBundleSaveModel` 保存 `bundle_sample.om`
- 通过 `aclmdlBundleLoadFromFile` 加载 Bundle，并逐个执行子模型
