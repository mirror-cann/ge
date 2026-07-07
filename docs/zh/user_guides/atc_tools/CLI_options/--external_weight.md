# --external\_weight

## 产品支持情况

全量芯片支持

## 功能说明

生成om离线模型文件时，是否将原始网络中的Const/Constant节点的权重外置，同时将节点类型转换为FileConstant类型。

离线场景，如果模型权重较大且环境对om离线模型大小有限制，建议开启外置权重将权重单独保存，来减小om大小。

## 关联参数

需要和[--output](--output.md)参数配合使用，生成的权重文件保存在与om离线模型文件同层级的weight目录下。

## 参数取值

- 0：（默认值）权重不外置，直接保存在om离线模型文件中。
- 1：权重外置但不归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型；不同节点权重保存到不同的文件中，以weight\_<hash值\>命名。
- 2：权重外置且归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型；所有权重保存到同一个文件中，以“--output参数指定的文件名\_weight\_combined”命名。

## 推荐配置及收益

无。

## 示例

以ONNX网络模型为例：

```bash
atc --framework=5 --model=$HOME/module/resnet50.onnx --output=$HOME/module/out/onnx_resnet50 --soc_version=<soc_version>  --external_weight=1 ...
```

## 依赖约束

权重外置场景，在开发推理应用、加载模型时：

- 若使用[aclmdlLoadFromFile](../../../api/graph_engine_api/c/acl/aclmdlLoadFromFile.md)接口加载模型，需将权重文件保存在与om离线模型文件同层级的weight目录下。
- 若使用[aclmdlSetConfigOpt](../../../api/graph_engine_api/c/acl/aclmdlSetConfigOpt.md)和[aclmdlLoadWithConfig](../../../api/graph_engine_api/c/acl/aclmdlLoadWithConfig.md)接口加载模型，对权重外置目录没有要求，后续加载模型时，通过[aclmdlLoadWithConfig](../../../api/graph_engine_api/c/acl/aclmdlLoadWithConfig.md)接口指定权重外置目录。
