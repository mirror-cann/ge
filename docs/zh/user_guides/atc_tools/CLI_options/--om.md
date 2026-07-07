# --om

## 产品支持情况

全量芯片支持

## 功能说明

离线模型（.om）、原始模型文件（例如Caffe框架的.prototxt，TensorFlow框架的.pb等）、GE dump图结构文件（.txt）的路径和文件名。

## 关联参数

- 若[--mode](--mode.md)取值为1：
  - 离线模型转换为JSON文件

    --om需要与[--mode](--mode.md)=1、[--json](--json.md)参数配合使用。

  - 原始模型文件转换为JSON文件

    --om需要与[--mode](--mode.md)=1、[--json](--json.md)、[--framework](--framework.md)参数配合使用。

- 若[--mode](--mode.md)取值为5：

    GE dump图结构文件转JSON文件，--om需要与[--mode](--mode.md)=5、[--json](--json.md)参数配合使用。

- 若[--mode](--mode.md)取值为6：

    针对已有的**离线模型**，显示模型信息等信息，则--om只需要与[--mode](--mode.md)参数配合使用。

## 参数取值

**参数值：**
离线模型（.om）、原始模型文件（例如Caffe框架的.prototxt，TensorFlow框架的.pb）或GE dump图结构文件（.txt）的路径。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

- 若[--mode](--mode.md)取值为1
  - 离线模型转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/out/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json ...
    ```

  - 原始模型文件转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/resnet50_tensorflow.pb  --json=$HOME/module/out/tf_resnet50.json --framework=3 ...
    ```

- 若[--mode](--mode.md)取值为5

    ```bash
    atc --mode=5 --om=$HOME/module/ge_proto_00000000_PreRunBegin.txt --json=$HOME/module/out/ge_proto.json ...
    ```

- 若[--mode](--mode.md)取值为6

    ```bash
    atc --mode=6 --om=$HOME/module/out/tf_resnet50.om ...
    ```

## 依赖约束

无。
