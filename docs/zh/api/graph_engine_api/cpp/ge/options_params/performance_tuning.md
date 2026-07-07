# 性能调优

## ge.exec.op\_precision\_mode

设置指定算子内部处理时的精度模式，支持指定一个算子或多个算子。通过该参数传入自定义的精度模式配置文件op\_precision.ini，可以为不同的算子设置不同的精度模式。

ini文件中按照算子类型、节点名称设置精度模式，每一行设置一个算子类型或节点名称的精度模式，按节点名称设置精度模式的优先级高于按算子类型。

配置文件中支持设置如下精度模式：

- high\_precision：表示高精度。
- high\_performance：表示高性能。
- enable\_float\_32\_execution：算子内部处理时使用FP32数据类型功能，该场景下FP32数据类型不会自动转换为HF32数据类型；若使用HF32计算，精度损失超过预期时，可启用该配置，指定部分算子内部计算时使用FP32，保持精度。

    <!-- npu="950,A3,910b" id1 -->
    **该选项仅支持以下产品类型：**

    <!-- npu="950" id6 -->
    Ascend 950PR/Ascend 950DT
    <!-- end id6 -->

    <!-- npu="910b" id7 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id7 -->

    <!-- npu="A3" id8 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id8 -->
    <!-- end id1 -->

- enable\_hi\_float\_32\_execution：算子内部处理时使用HF32数据类型功能，启用此配置后，FP32数据类型自动转换为HF32数据类型；该机制可以降低数据所占空间大小，实现性能提升。

    **当前版本该选项暂不支持。**

- support\_out\_of\_bound\_index：表示对gather、scatter和segment类算子的indices输入进行越界校验，校验会降低算子的执行性能。
- keep\_fp16：算子内部处理时使用FP16数据类型功能，该场景下FP16数据类型不会自动转换为FP32数据类型；若使用FP32计算时性能不满足预期，同时精度要求不高情况下，可以选择keep\_fp16模式，**牺牲精度提升性能，不建议使用该低精度模式**。
- super\_performance：表示超高性能，和高性能相比，在算法计算公式上进行了优化。

具体某个算子支持配置的精度/性能模式取值，可以通过CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/all\_ops\_impl\_mode.ini文件查看。

op\_precision.ini配置样例如下：

```ini
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

**配置示例：**

```c++
{"ge.exec.op_precision_mode", "$HOME/conf/op_precision.ini"};
```

**必选/可选**：可选

**生效级别**：全局

## ge.exec.variable\_acc

是否开启变量格式优化。

**参数取值：**

- True：（默认值）开启。
- False：关闭。

为了提高训练效率，在网络执行的变量初始化过程中，将变量转换成更适合在AI处理器上运行的数据格式。但在用户特殊要求场景下，可以选择关闭该功能开关。

**使用约束**：

开启时，不能同时将ge.AllowMultiGraphParallelCompile配置为"1"，否则会校验报错。

**配置示例：**

```c++
{"ge.exec.variable_acc", "True"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.graphMaxParallelModelNum

图执行模式下，同一个图可以在同一个device上被多个模型并行加载执行；该参数用于控制允许并行加载的最大模型数目。

**参数取值：**

1\~INT32\_MAX，默认为8。

**配置示例：**

```c++
{"ge.graphMaxParallelModelNum", "8"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.tiling\_schedule\_optimize

Tiling下沉调度优化开关。

由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，该过程称之为Tiling；根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，故一般在Host侧CPU上执行，但是满足下述条件Tiling实现会下沉到Device侧执行：

1. 模型为静态shape。
2. 模型中的算子支持Tiling下沉，比如FusedInferAttentionScore、IncreFlashAttention等融合算子。
3. 支持Tiling下沉的算子值有依赖，需要满足前一个算子的值有device的执行结果；如果依赖的值是Const，则不需要下沉执行Tiling，编译时会完成Tiling。

<!-- npu="A3,910b,310p" id2 -->
该参数仅支持以下产品：

<!-- npu="910b" id3 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id3 -->

<!-- npu="A3" id4 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id4 -->

<!-- npu="310p" id5 -->
Atlas 推理系列产品
<!-- end id5 -->
<!-- end id2 -->

**参数取值：**

- 0：（默认值）关闭Tiling下沉。
- 1：开启Tiling下沉。

**配置示例：**

```c++
{"ge.tiling_schedule_optimize", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph
