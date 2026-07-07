# 基础功能

## ge.constLifecycle

用于训练和在线推理场景下配置常量节点的生命周期。

**参数取值：**

- session：按照session级别存储常量节点。配置为session级别时，session内多张图间的常量节点支持内存复用，但需要用户保证多张图上同名的常量节点完全相同。
- graph：按照graph级别存储常量节点，后续用户可调用[SetGraphConstMemoryBase](../Session/SetGraphConstMemoryBase.md)接口按图级别对const内存自行管理。

训练场景默认值为session；在线推理场景默认值为graph。

**配置示例：**

```c++
{"ge.constLifecycle", "graph"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.deterministic

是否开启确定性计算。

默认情况下，不开启确定性计算，算子在相同的硬件和输入下，多次执行的结果可能不同。这个差异的来源，一般是因为在算子实现中，存在异步的多线程执行，会导致浮点数累加的顺序变化。当开启确定性计算功能时，算子在相同的硬件和输入下，多次执行将产生相同的输出。但启用确定性计算往往导致算子执行变慢。当发现模型多次执行结果不同，或者是进行精度调优时，可开启确定性计算，辅助模型调试、调优。

**参数取值：**

- 0：（默认值）不开启确定性计算。
- 1：开启确定性计算。

**配置示例：**

```c++
{"ge.deterministic", "0"};
```

**必选/可选**：可选

**生效级别**：全局

## ge.enableSingleStream

静态shape场景下，是否启用图运行顺序单流串行执行。

其中，流（Stream）用于维护一些异步操作的执行顺序，确保按照应用程序中的代码调用顺序在Device上执行。

**参数取值：**

- true：表示启用，图运行顺序单流串行执行。
- false：（默认值）表示关闭，图运行时多流并行执行。

**配置示例：**

```c++
{"ge.enableSingleStream", "false"};
```

**必选/可选**：可选

**生效级别**：graph

**使用约束：**

模型中存在Cmo算子和如下控制类算子时，不能使用单Stream特性，只能使用默认值false。

- Merge
- Switch
- Enter
- RefEnter

## ge.exec.deviceId

GE实例运行时操作设备的逻辑ID。

**参数取值：**

- 在线推理场景，限制有限范围为-1\~N-1，默认值为-1。
- 训练场景，限制有限范围为0\~N-1，默认值为0。

N表示该台Server上的可用AI处理器个数。

**配置示例：**

```c++
{"ge.exec.deviceId", "-1"};
```

**必选/可选**：可选

**生效级别**：全局

## ge.exec.frozenInputIndexes

设置地址不刷新的输入Tensor的索引。该参数仅限于[LoadGraph](../Session/LoadGraph.md)调用。针对不同模型，输入Tensor索引内容不同：

- 动态shape模型：需要传递输入Tensor的索引、数据在Device上的地址、数据长度，其中地址要求十进制表示。
- 静态shape模型：只需要传递输入Tensor的索引即可；传其他参数比如数据长度参数，不生效。

**配置示例：**

```c++
# 只传输入Tensor索引
{"ge.exec.frozenInputIndexes", "0;1;2"};
# 传递输入Tensor索引、数据在Device上的地址、数据长度
{"ge.exec.frozenInputIndexes", "0,88832131,4;1,888213294,4;2,193492421,2"};
```

详细使用示例以及使用注意事项请参见[单进程单卡异步运行](../../../../../user_guides/graph_dev/compile_and_run_graph/run_graph_asynchronously.md#单进程单卡异步运行)。

**必选/可选**：可选

**生效级别**：graph

**使用约束**：

地址不刷新的输入Tensor，必须是静态shape，针对动态shape模型，该输入节点Tensor也必须是静态shape。

## ge.exec.hostInputIndexes

随路拷贝场景，设置placement为Host的输入Tensor索引，多个输入的Tensor索引使用英文分号分隔。

随路拷贝是指随着模型内算子地址刷新动作，将Host侧输入的Tensor内存一起拷贝到Device侧。

**配置示例：**

```c++
{"ge.exec.hostInputIndexes", "0;1;2"};
```

**必选/可选**：可选

**生效级别**：graph

**使用约束：**

- 该参数仅适用于静态shape模型。
- 随路拷贝特性不适合大数据量的输入，性能可能会有劣化。
- 若模型只有一个输入，不支持和ge.exec.frozenInputIndexes同时使用；若模型有多个输入，不支持和ge.exec.frozenInputIndexes指定同一个输入。
- 通过options配置项加载该参数后，后续的执行接口不支持使用[RunGraphAsync](../Session/RunGraphAsync.md)。

## ge.exec.rankTableFile

用于描述参与集合通信的集群信息，包括Server，Device，容器等的组织信息，填写ranktable文件路径，包含文件路径和文件名。

**配置示例：**

```c++
{"ge.exec.rankTableFile", "./yourfilepath/ranktable.json"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.exec.rankId

指当前进程在group中对应的rank ID，用于分布式训练和集合通信，标识多卡训练中的当前设备。对于用户自定义group，rank在本group内从0开始进行重排；对于hccl world group，rank id和world rank id相同。该参数需要与ge.exec.rankTableFile配合使用。

**参数取值：**

字符串形式的整数：

- world rank id，指进程在hccl world group中对应的rank标识序号，范围：0\~（rank size-1）。
- local rank id，指group内进程在其所在Server内的rank编号，范围：0\~（local rank size-1）。

**配置示例：**

```c++
{"ge.exec.rankId", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.graphRunMode

图执行模式。

**参数取值：**

- 0：（默认值）在线推理场景下，请配置为0。
- 1：训练场景下，请配置为1。

**配置示例：**

```c++
{"ge.graphRunMode", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.outputDatatype

指定图的输出数据类型。

**参数取值：**

- FP32
- FP16
- UINT8
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
- FP8E5M2：仅Ascend 950PR/Ascend 950DT支持该类型。
- FP8E4M3FN：仅Ascend 950PR/Ascend 950DT支持该类型。
<!-- end id1 -->
图编译后，在对应的子图文件中，上述数据类型分别呈现方式如下：

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

**参数值约束：**

若不指定具体数据类型，则以图最后一层输出的节点（算子）数据类型为准；若指定了类型，则以该参数指定的类型为准。

**配置示例：**

```c++
{"ge.outputDatatype", "FP32"};
```

**必选/可选**：可选

**生效级别**：session/graph

## ge.session\_device\_id

当用户需要将不同的模型通过同一个脚本在不同的Device上执行，可以通过该参数指定Device的逻辑ID。

通常可以创建多个线程，每个线程是不同的Session，每个Session传不同的ge.session\_device\_id。

**配置示例：**

```c++
{"ge.session_device_id", "0"};
```

**必选/可选**：可选

**生效级别**：session

## ge.socVersion

指定编译优化模型的AI处理器型号。

取值查询方法如下：

<!-- npu="950" id2 -->
- 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascend_xxx_，**NPU Name**取值为1234，实际配置值为Ascend_xxx__\__1234。

    其中，id为设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
<!-- end id2 -->

<!-- npu="A3" id3 -->
- 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_** -c **_chip\_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascend_xxx_，**NPU Name**取值为1234，实际配置值为Ascend_xxx__\__1234。其中：
  - id：设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
  - chip\_id：芯片id，通过**npu-smi info -m**命令查出的Chip ID即为芯片id。
<!-- end id3 -->

<!-- npu="910b,910,310p" id4 -->
- 针对如下产品：在安装AI处理器的服务器执行**npu-smi info**命令进行查询，获取**Name**信息。实际配置值为AscendName，例如**Name**取值为_xxxyy_，实际配置值为Ascend_xxxyy_。

    <!-- npu="910b" id5 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id5 -->

    <!-- npu="310p" id6 -->
    Atlas 推理系列产品
    <!-- end id6 -->

    <!-- npu="910" id7 -->
    Atlas 训练系列产品
    <!-- end id7 -->
<!-- end id4 -->

**必选/可选**：可选

**生效级别**：全局/session/graph
