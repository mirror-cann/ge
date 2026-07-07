# 基础功能

## INPUT\_FORMAT

输入数据格式。此参数实际对应的options参数为`input_format`。

**参数取值：**

支持NCHW、NHWC、ND、NCDHW、NDHWC格式。

**配置示例：**

```c++
{ge::ir_option::INPUT_FORMAT, "NHWC"}
```

如果同时开启AIPP，在进行推理业务时，输入图片数据要求为NHWC排布。该场景下INPUT\_FORMAT参数指定的数据格式不生效。

> [!NOTE]说明
>该参数仅针对动态batch\_size、动态分辨率和动态维度场景。
>上述场景下，INPUT\_FORMAT必须设置并且和所有Data算子的format保持一致，否则会导致模型编译失败。

**产品支持情况：**

全量芯片支持。

## OP\_NAME\_MAP

扩展算子（非标准算子）映射配置文件路径和文件名，不同的网络中某扩展算子的功能不同，可以指定该扩展算子到具体网络中实际运行的扩展算子的映射。此参数实际对应的options参数为`op_name_map`。

Caffe网络中具有相同类型名但计算功能不同的层，比如DetectionOutput层，需要使用算子映射指明为FSRDetectionOutput、SSDDetectionOutput等检测算子类型，否则生成离线模型会执行失败。

该参数仅适用与Caffe框架。

路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

**配置示例**：

```c++
{ge::ir_option::OP_NAME_MAP, "/home/test/opname_map.cfg"}
```

其中，扩展算子映射配置文件内容示例如下：

```text
OpA:Network1OpA
```

**产品支持情况：**

<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/basic_functions_res.md#id1 -->

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->

## INSERT\_OP\_FILE

输入预处理算子的配置文件路径，例如Aipp算子。参数详细使用请参见[更多特性 \> AIPP](../../../../../user_guides/graph_dev/more_features/AIPP.md)。此参数实际对应的options参数为`ge.insertOpFile`。

若配置了该参数，则不能对同一个输入节点同时使用INPUT\_FP16\_NODES参数。

配置文件路径：支持大小写字母、数字，下划线；文件名部分：支持大小写字母、数字，下划线和点\(.\)

**配置示例**：

```c++
{ge::ir_option::INSERT_OP_FILE, "/home/test/aipp.cfg"}
```

配置文件的内容示例如下：

```textproto
aipp_op {
    aipp_mode:static
    input_format:YUV420SP_U8
    csc_switch:true
    var_reci_chn_0:0.00392157
    var_reci_chn_1:0.00392157
    var_reci_chn_2:0.00392157
}
```

> [!NOTE]说明
>
>配置文件详细说明，请参考[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。

**产品支持情况：**

<!-- npu="950" id8 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id8 -->
<!-- npu="A3" id9 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id9 -->
<!-- npu="910b" id10 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id10 -->
<!-- npu="310b" id11 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id11 -->
<!-- npu="310p" id12 -->
- Atlas 推理系列产品：支持
<!-- end id12 -->
<!-- npu="910" id13 -->
- Atlas 训练系列产品：支持
<!-- end id13 -->
<!-- npu="IPV350" id14 -->
- IPV350：不支持
<!-- end id14 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/basic_functions_res.md#id2 -->

## OUTPUT\_TYPE

网络输出数据类型。此参数实际对应的options参数为`ge.outputDatatype`。

**参数取值：**

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
<!-- npu="950" id15 -->
- HIF8：仅Ascend 950PR/Ascend 950DT支持该类型。
- FP8E5M2：仅Ascend 950PR/Ascend 950DT支持该类型。
- FP8E4M3FN：仅Ascend 950PR/Ascend 950DT支持该类型。
<!-- end id15 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/basic_functions_res.md#id3 -->

模型编译后，在对应的\*.om模型文件中，上述数据类型分别呈现方式如下：

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

**配置示例：**

```c++
{ge::ir_option::OUTPUT_TYPE, "FP32"}
```

**参数值约束：**

- 若不指定具体数据类型，则以原始网络模型最后一层输出的算子数据类型为准。
- 若指定了类型，则以该参数指定的类型为准。

**产品支持情况：**

全量芯片支持。

## INPUT\_FP16\_NODES

指定输入数据类型为FP16的输入节点名称。此参数实际对应的options参数为`ge.INPUT_NODES_SET_FP16`。

例如："node\_name1;node\_name2"，指定的节点必须放在双引号中，节点中间使用英文分号分隔。若配置了该参数，则不能对同一个输入节点同时使用INSERT\_OP\_FILE参数。

配置示例：

```c++
{ge::ir_option::INPUT_FP16_NODES, "node_name1;node_name2"}
```

**产品支持情况：**

全量芯片支持。
