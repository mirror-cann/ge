# 性能调优

## OP\_PRECISION\_MODE

设置指定算子内部处理时的精度模式，支持指定一个算子或多个算子。通过该参数传入自定义的精度模式配置文件op\_precision.ini，可以为不同的算子设置不同的精度模式。此参数实际对应的options参数为`ge.exec.op_precision_mode`。

**配置示例：**

```c++
{ge::ir_option::OP_PRECISION_MODE, "op_precision.ini"}
```

配置文件中支持设置如下精度模式：

- high\_precision：表示高精度。
- high\_performance：表示高性能。
<!-- npu="A3,910b" id1 -->
- enable\_float\_32\_execution：算子内部处理时使用FP32数据类型功能，该场景下FP32数据类型不会自动转换为HF32数据类型；若使用HF32计算，精度损失超过预期时，可启用该配置，指定部分算子内部计算时使用FP32，保持精度。

    **该选项仅支持以下产品类型：**

    <!-- npu="A3" id2 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id2 -->

    <!-- npu="910b" id3 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id3 -->

<!-- end id1 -->

<!-- npu="A3,910b" id4 -->
- enable\_hi\_float\_32\_execution：算子内部处理时使用HF32数据类型功能，启用此配置后，FP32数据类型自动转换为HF32数据类型；该机制可以降低数据所占空间大小，实现性能提升。

    **当前版本该选项暂不支持。**
<!-- end id4 -->

- support\_out\_of\_bound\_index：表示对gather、scatter和segment类算子的indices输入进行越界校验，校验会降低算子的执行性能。
- keep\_fp16：算子内部处理时使用FP16数据类型功能，该场景下FP16数据类型不会自动转换为FP32数据类型；若使用FP32计算时性能不满足预期，同时精度要求不高情况下，可以选择keep\_fp16模式，**牺牲精度提升性能，不建议使用该低精度模式**。
- super\_performance：表示超高性能，和高性能相比，在算法计算公式上进行了优化。

具体某个算子支持配置的精度/性能模式取值，可以通过CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/all\_ops\_impl\_mode.ini文件查看。

**样例如下**：ini文件中按照算子类型、节点名称设置精度模式，每一行设置一个算子类型或节点名称的精度模式，**按节点名称设置精度模式的优先级高于按算子类型**。

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

**使用约束：**

- 该参数不能与OP\_SELECT\_IMPL\_MODE、OPTYPELIST\_FOR\_IMPLMODE参数同时使用，若三个参数同时配置，则只有OP\_PRECISION\_MODE参数指定的模式生效。
<!-- npu="A3,910b" id5 -->
- 针对同一个算子，如果通过OP\_PRECISION\_MODE数配置了enable\_hi\_float\_32\_execution或enable\_float\_32\_execution，该场景下不建议再与ALLOW\_HF32参数同时使用，若同时使用，则优先级如下：

    op\_precision\_mode\(ByNodeName，按节点名称设置精度模式\) \> allow\_hf32 \> op\_precision\_mode\(ByOpType，按算子类型设置精度模式\)
<!-- end id5 -->

- 该参数不建议配置，若使用高性能或者高精度模式，网络性能或者精度不是最优，则可以使用该参数，通过配置ini文件调整某个具体算子的精度模式。

**产品支持情况：**

全量芯片支持。

<!-- npu="950,A3,910b,910,310p,310b,IPV350" id6 -->
## TILING\_SCHEDULE\_OPTIMIZE

Tiling下沉调度优化开关。此参数实际对应的options参数为`ge.tiling_schedule_optimize`。

由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，该过程称之为Tiling；根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，故一般在Host侧CPU上执行，但是满足下述条件Tiling实现会下沉到Device侧执行：

1. 模型为静态shape。
2. 模型中的算子支持Tiling下沉，比如FusedInferAttentionScore、IncreFlashAttention等融合算子。
3. 支持Tiling下沉的算子值有依赖，需要满足前一个算子的值有device的执行结果；如果依赖的值是Const，则不需要下沉执行Tiling，编译时会完成Tiling。

**参数取值：**

- 0：关闭Tiling下沉，默认为0。
- 1：开启Tiling下沉。

**配置示例：**

```c++
{ge::ir_option::TILING_SCHEDULE_OPTIMIZE, "1"}
```

**产品支持情况：**

<!-- npu="950" id7 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id7 -->
<!-- npu="A3" id8 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id8 -->
<!-- npu="910b" id9 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id9 -->
<!-- npu="310b" id10 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id10 -->
<!-- npu="310p" id11 -->
- Atlas 推理系列产品：支持
<!-- end id11 -->
<!-- npu="910" id12 -->
- Atlas 训练系列产品：不支持
<!-- end id12 -->
<!-- npu="IPV350" id13 -->
- IPV350：不支持
<!-- end id13 -->
<!-- end id6 -->
