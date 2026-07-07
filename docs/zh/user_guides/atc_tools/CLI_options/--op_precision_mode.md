# --op\_precision\_mode

## 产品支持情况

全量芯片支持

## 功能说明

设置指定算子内部处理时的精度模式，支持指定一个算子或多个算子。

## 关联参数

- 该参数不能与[--op\_select\_implmode](--op_select_implmode.md)、[--optypelist\_for\_implmode](--optypelist_for_implmode.md)参数同时使用，若三个参数同时配置，则只有[--op\_precision\_mode](--op_precision_mode.md)参数指定的模式生效。
<!-- npu="A3,910b" id4 -->
- 针对同一个算子，如果通过[--op\_precision\_mode](--op_precision_mode.md)参数配置了enable\_hi\_float\_32\_execution或enable\_float\_32\_execution，该场景下不建议再与[--allow\_hf32](--allow_hf32.md)参数同时使用，若同时使用，则优先级如下：

    op\_precision\_mode\(ByNodeName，按节点名称设置精度模式\) \> allow\_hf32 \> op\_precision\_mode\(ByOpType，按算子类型设置精度模式\)
<!-- end id4 -->

关联参数示意图如[图1](#fig1)所示。

**图 1**  关联参数示意图<a id="fig1"></a>
![图示](../figures/related_params.png "关联参数示意图")

设置具体算子精度模式场景下：

1. 首先读取[--op\_precision\_mode](--op_precision_mode.md)参数，校验该参数的ini配置文件是否存在，若存在则解析文件并读取算子的精度模式，否则上报异常。
2. [--op\_precision\_mode](--op_precision_mode.md)不存在则读取[--op\_select\_implmode](--op_select_implmode.md)参数：
    1. 首先检查是否配置为high\_xxx\_for\_all参数，若是则解析high\_xxx\_for\_all.ini文件并读取算子的精度模式。
    2. 若配置为high\_xxx参数，则检查是否配置[--optypelist\_for\_implmode](--optypelist_for_implmode.md)参数，若是，则读取该参数配置的算子精度模式；否则解析high\_xxx.ini文件并读取算子的精度模式。

## 参数取值

**参数值：**
设置算子精度模式的配置文件（.ini格式）路径以及文件名，配置文件中支持设置如下精度模式：

- high\_precision：表示高精度。
- high\_performance：表示高性能。
<!-- npu="950,A3,910b" id5 -->
- enable\_float\_32\_execution：算子内部处理时使用FP32数据类型功能，该场景下FP32数据类型不会自动转换为HF32数据类型；若使用HF32计算，精度损失超过预期时，可启用该配置，指定部分算子内部计算时使用FP32，保持精度。

    **该选项仅支持以下产品类型：**

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT
    <!-- end id1 -->

    <!-- npu="910b" id2 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id2 -->

    <!-- npu="A3" id3 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id3 -->
<!-- end id5 -->

<!-- npu="950,A3,910b" id6 -->
- enable\_hi\_float\_32\_execution：算子内部处理时使用HF32数据类型功能，启用此配置后，FP32数据类型自动转换为HF32数据类型；该机制可以降低数据所占空间大小，实现性能提升。**当前版本该选项暂不支持。**
<!-- end id6 -->

- support\_out\_of\_bound\_index：表示对gather、scatter和segment类算子的indices输入进行越界校验，校验会降低算子的执行性能。
- keep\_fp16：算子内部处理时使用FP16数据类型功能，该场景下FP16数据类型不会自动转换为FP32数据类型；若使用FP32计算时性能不满足预期，同时精度要求不高情况下，可以选择keep\_fp16模式，**牺牲精度提升性能，不建议使用该低精度模式**。
- super\_performance：表示超高性能，和高性能相比，在算法计算公式上进行了优化。

具体某个算子支持配置的精度/性能模式取值，可以通过CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/all\_ops\_impl\_mode.ini文件查看。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束**：

- 当前仅支持通过.ini配置文件方式设置算子精度，配置文件中的内容以key-value（算子类型=精度模式）形式呈现，每一行设置一个算子的精度模式。
- 算子类型必须为基于Ascend IR定义的算子的OpType，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。

## 推荐配置及收益

- 该参数不建议配置，若使用高性能或者高精度模式，网络性能或者精度不是最优，则可以使用该参数，通过配置ini文件调整具体某个算子的精度模式。
- 通过该参数加载的ini配置文件，建议使用[--op\_select\_implmode](--op_select_implmode.md)参数用户另存后的ini配置文件，详情请参见[推荐配置及收益](--op_select_implmode.md#推荐配置及收益)。

## 示例

构造算子精度模式配置文件_op\_precision.ini_，并在该文件中按照算子类型、节点名称设置精度模式，每一行设置一个算子类型或节点名称的精度模式**，按节点名称设置精度模式的优先级高于按算子类型**。

配置样例如下：

<!-- npu="950,A3,910b,910,310p,310b" id7 -->
```text
[ByOpType]
optype1=high_precision
optype2=high_performance
optype3=enable_hi_float_32_execution
optype4=support_out_of_bound_index

[ByNodeName]
nodename1=high_precision
nodename2=high_performance
nodename3=enable_hi_float_32_execution
nodename4=support_out_of_bound_index
```
<!-- end id7 -->

<!-- npu="IPV350" id8 -->
```text
[ByOpType]
optype1=high_precision
optype2=high_performance
optype4=support_out_of_bound_index

[ByNodeName]
nodename1=high_precision
nodename2=high_performance
nodename4=support_out_of_bound_index
```
<!-- end id8 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--op_precision_mode_res.md#id1 -->

将配置好的_op\_precision.ini_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/conf，使用示例如下：

```bash
atc --op_precision_mode=$HOME/conf/op_precision.ini ...
```

## 依赖约束

无。
