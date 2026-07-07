# --json

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
全量芯片支持
<!-- end id2 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--json_res.md#id1 -->

<!-- npu="IPV350" id1 -->
IPV350：不支持
<!-- end id1 -->

## 功能说明

离线模型、原始模型文件、GE dump图结构文件转换为JSON文件的路径和文件名。

如果是已有的离线模型转换为JSON文件，在转换后的文件中还可以查看原始模型转换为该离线模型时，使用的基础版本号（比如ATC软件版本信息，OPP算子包版本信息等）以及当时模型转换使用的atc命令。

## 关联参数

- 离线模型转换为JSON文件

    该参数需要与[--mode](--mode.md)=1、[--om](--om.md)参数配合使用。

- 原始模型文件转换为JSON文件

    该参数需要与[--mode](--mode.md)=1、[--om](--om.md)参数、[--framework](--framework.md)配合使用。

    原始模型为MindSpore框架时，即--framework=1时，不支持转换为JSON文件。

- GE dump图结构文件转JSON文件

    该参数需要与[--mode](--mode.md)=5、[--om](--om.md)参数配合使用。

    仅支持dump出的ge\_proto\*.txt格式文件转成JSON文件。

## 参数取值

**参数值：**
JSON文件的路径和文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

- 离线模型转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/out/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json ...
    ```

- 原始模型文件转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/resnet50_tensorflow.pb  --json=$HOME/module/out/tf_resnet50.json  --framework=3 ...
    ```

- GE dump图结构文件转JSON文件

    ```bash
    atc --mode=5 --om=$HOME/module/ge_proto_00000000_PreRunBegin.txt --json=$HOME/module/out/ge_proto.json ...
    ```

## 依赖约束

无。
