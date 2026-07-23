# --output\_type

## 产品支持情况

全量芯片支持

## 功能说明

指定网络输出数据类型或指定某个输出节点的输出类型。

## 关联参数

若指定某个输出节点的输出类型，则需要和[--out\_nodes](--out_nodes.md)参数配合使用。

## 参数取值

**参数值：**

- FP32：推荐分类网络、检测网络使用。
- FP16：推荐分类网络、检测网络使用。通常用于一个网络输出作为另一个网络输入场景。
- UINT8：图像超分辨率网络，推荐使用，推理性能更好。
- INT8
- UINT16
- INT16
- UINT32
- INT32
- UINT64
- INT64
- DOUBLE
<!-- npu="950" id1 -->
- HIF8：仅Ascend 950PR/Ascend 950DT支持该类型。
<!-- end id1 -->
<!-- npu="950" id2 -->
- FP8E5M2：仅Ascend 950PR/Ascend 950DT支持该类型。
<!-- end id2 -->
<!-- npu="950" id3 -->
- FP8E4M3FN：仅Ascend 950PR/Ascend 950DT支持该类型。
<!-- end id3 -->

<!-- npu="IPV350" id4 -->
其中，HIF8、FP8E5M2、FP8E4M3FN：IPV350不支持。
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--output_type_res.md#id1 -->

**参数值约束：**

模型转换完毕，在对应的om离线模型文件中，上述数据类型对应方式为：

- DT\_FLOAT
- DT\_FLOAT16
- DT\_UINT8
- DT\_INT8
- DT\_UINT16
- DT\_INT16
- DT\_UINT32
- DT\_INT32
- DT\_UINT64
- DT\_INT64
- DT\_DOUBLE
- DT\_HIFLOAT8
- DT\_FLOAT8\_E5M2
- DT\_FLOAT8\_E4M3FN

若在模型转换时不指定网络具体输出数据类型，则以原始网络模型最后一层输出的算子数据类型为准；若指定了类型，则以该参数指定的类型为准，此时[--is\_output\_adjust\_hw\_layout](--is_output_adjust_hw_layout.md)参数指定的类型不生效。

## 推荐配置及收益

无。

## 示例

- 指定网络输出数据类型

    ```bash
    atc --output_type=FP32 ...
    ```

- 指定某个输出节点的输出数据类型

    例如：--output\_type="node1:0:FP16;node2:0:FP32"，表示node1节点第一个输出设置为FP16，node2第一个节点输出设置为FP32。指定的节点必须放在双引号中，节点中间使用英文分号分隔。

    该场景下，该参数需要与[--out\_nodes](--out_nodes.md)参数配合使用。

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow.pb --framework=3 --output=$HOME/module/out/tf_resnet50  --soc_version=<soc_version>  --output_type="conv1:0:FP16"  --out_nodes="conv1:0" ...
    ```

## 依赖约束

无。
