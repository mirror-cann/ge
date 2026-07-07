# 基础功能

## CLUSTER\_CONFIG

设置**集群**配置文件路径，用于配置目标执行逻辑设备信息以生成HCCL任务，多卡分布式训练场景下使用。此参数实际对应的options参数为`ge.cluster_config`。

若模型中包含通信算子（当前仅支持AllGather通信算子），该参数必填。

**配置示例：**

```c++
{ge::ir_option::CLUSTER_CONFIG, "/home/test/cluster_config.json"}
```

cluster\_config.json配置示例如下：

```json
{
    "RankTable": {
        "status": "completed",
        "version": "1.2",
        "server_count": "2",
        "server_list": [
            {
                "server_id": "node_0",
                "device": [
                    {
                        "device_id": "0",
                        "rank_id": "0"
                    }
                ]
            },
            {
                "server_id": "node_1",
                "device": [
                    {
                        "device_id": "0",
                        "rank_id": "1"
                    }
                ]
            }
        ]
    },
    "HcclCommConfig": {
        "hcclCommName": "global name"
    }
}
```

参数说明解释如下：

- RankTable：配置的核心，定义了参与计算的节点和rank信息。
  - status：必选，rank table可用标识。
    - completed：表示rank table可用。
    - initializing：表示rank table不可用。

  - version：必选，rank table模板版本信息，请配置为：1.2。
  - server\_count：可选，参与集合通信的AI Server个数。
  - server\_list：必选，参与集合通信的AI Server列表。
    - server\_id：必选，AI Server标识，字符串类型，长度小于等于64，请确保全局唯一。配置示例：node\_0。
    - device：必选，Device列表。
      - device\_id：必选，AI处理器的物理ID，即Device在AI Server上的序列号。
      - rank\_id：必选，rank唯一标识，请配置为整数，从0开始配置，且全局唯一，取值范围：\[0, 总Device数量-1\]。

- HcclCommConfig：HCCL通信层的全局配置信息。
  - hcclCommName：必选，全局通信组的名称或标识符。

**产品支持情况：**

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
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：不支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：不支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/basic_functions_res.md#id1 -->

## DETERMINISTIC

是否开启确定性计算。此参数实际对应的options参数为`ge.deterministic`。

默认情况下，不开启确定性计算，算子在相同的硬件和输入下，多次执行的结果可能不同。这个差异的来源，一般是因为在算子实现中，存在异步的多线程执行，会导致浮点数累加的顺序变化。当开启确定性计算功能时，算子在相同的硬件和输入下，多次执行将产生相同的输出。

通常建议不开启确定性计算，因为确定性计算往往会导致算子执行变慢，进而影响性能。当发现模型多次执行结果不同，或者是进行精度调优时，可开启确定性计算，辅助模型调试、调优。

**参数取值：**

- 0：（默认值）不开启确定性计算。
- 1：开启确定性计算。

**配置示例：**

```c++
{ge::ir_option::DETERMINISTIC, "1"}
```

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
- Atlas 200I/500 A2 推理产品：不支持
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
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/basic_functions_res.md#id2 -->

## ENABLE\_SINGLE\_STREAM

静态shape场景下，是否启用模型推理顺序单流串行执行。此参数实际对应的options参数为`ge.enableSingleStream`。

其中，流（Stream）用于维护一些异步操作的执行顺序，确保按照应用程序中的代码调用顺序在Device上执行。

**参数取值：**

- true：表示启用，模型推理顺序单流串行执行。
- false：（默认值）表示关闭，模型推理时多流并行执行。

**参数值约束：**

模型中存在Cmo算子和如下控制类算子时，不能使用单Stream特性，只能使用默认值false。

- Merge
- Switch
- Enter
- RefEnter

**配置示例：**

```c++
{ge::ir_option::ENABLE_SINGLE_STREAM, "true"}
```

**产品支持情况：**

<!-- npu="950" id15 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id15 -->
<!-- npu="A3" id16 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id16 -->
<!-- npu="910b" id17 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id17 -->
<!-- npu="310b" id18 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id18 -->
<!-- npu="310p" id19 -->
- Atlas 推理系列产品：支持
<!-- end id19 -->
<!-- npu="910" id20 -->
- Atlas 训练系列产品：支持
<!-- end id20 -->
<!-- npu="IPV350" id21 -->
- IPV350：不支持
<!-- end id21 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/basic_functions_res.md#id3 -->

## HCCL\_SUB\_COMM\_CONFIG

设置HCCL**子通信域**配置文件路径，用于配置HCCL子通信参数。多卡分布式训练场景下使用。此参数实际对应的options参数为`ge.hccl_sub_comm_config`。

若模型中包含通信算子（当前仅支持AllGather通信算子），该参数必填。若模型算子包含子通信域参数，但在模型编译时未指定HCCL\_SUB\_COMM\_CONFIG参数，系统将默认依据集群的实际业务配置执行。

**配置示例：**

```c++
{ge::ir_option::CLUSTER_CONFIG, "/home/test/cluster_config.json"}
{ge::ir_option::HCCL_SUB_COMM_CONFIG, "/home/test/sub_comm_config.json"}
```

sub\_comm\_config.json文件示例如下：

```json
{
  "GroupList": [
    {
      "RankIds": [0,1],
      "HcclCommconfig": {
        "hcclCommName": "test_group"
      }
    }
  ]
}
```

其中：

- RankIds：rank唯一标识，请配置为整数，从0开始配置，且全局唯一，取值范围：\[0, 总Device数量-1\]。

    **建议rank\_id按照Device物理连接顺序进行排序，即将物理连接上较近的Device编排在一起，否则可能会对性能造成影响。**

    例如，若device\_ip按照物理连接从小到大设置，则rank\_id也建议按照从小到大的顺序设置。

- hcclCommName：通信组命名，供程序调用和区分。

**产品支持情况：**

<!-- npu="950" id22 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id22 -->
<!-- npu="A3" id23 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id23 -->
<!-- npu="910b" id24 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id24 -->
<!-- npu="310b" id25 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id25 -->
<!-- npu="310p" id26 -->
- Atlas 推理系列产品：不支持
<!-- end id26 -->
<!-- npu="910" id27 -->
- Atlas 训练系列产品：不支持
<!-- end id27 -->
<!-- npu="IPV350" id28 -->
- IPV350：不支持
<!-- end id28 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/basic_functions_res.md#id4 -->

## OPTION\_HOST\_ENV\_OS

此参数实际对应的options参数为`ge.host_env_os`。

若模型编译环境的操作系统及其架构与模型运行环境不一致时，则需使用本参数设置模型运行环境的操作系统类型。如果不设置，则默认取模型编译环境的操作系统类型。

模型编译环境的操作系统及其架构与模型运行环境不一致时，需要与OPTION\_HOST\_ENV\_CPU参数配合使用，通过OPTION\_HOST\_ENV\_OS参数设置操作系统类型、通过OPTION\_HOST\_ENV\_CPU参数设置操作系统架构。

**参数取值：**

查看`${INSTALL_DIR}/opp/built-in/op_graph/lib/`下打包的算子so的OS类型。

**参数默认值：**

查看$\{INSTALL\_DIR\}/opp/scene.info文件中的取值。

`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**配置示例：**

```c++
{ge::OPTION_HOST_ENV_OS, "linux"},
{ge::OPTION_HOST_ENV_CPU, "x86_64"}
```

**产品支持情况：**

全量芯片支持。

## OPTION\_HOST\_ENV\_CPU

此参数实际对应的options参数为`ge.host_env_cpu`。

若模型编译环境的操作系统及其架构与模型运行环境不一致时，则需使用本参数设置模型运行环境的操作系统架构。如果不设置，则默认取模型编译环境的操作系统架构。

如果模型编译环境的操作系统及其架构与模型运行环境不一致，需要与OPTION\_HOST\_ENV\_OS参数配合使用，通过OPTION\_HOST\_ENV\_OS参数设置操作系统类型、通过OPTION\_HOST\_ENV\_CPU参数设置操作系统架构。

**参数取值：**

查看`${INSTALL_DIR}/opp/built-in/op_graph/lib/`下打包的算子so的OS/CPU类型。

**参数默认值**：查看$\{INSTALL\_DIR\}/opp/scene.info文件中的取值。

`${INSTALL_DIR}`请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**配置示例：**

```c++
{ge::OPTION_HOST_ENV_OS, "linux"},
{ge::OPTION_HOST_ENV_CPU, "x86_64"}
```

- 若转换后的离线模型包含操作系统类型、架构，例如：_xxx\_linux\_x86\_64_.om，则说明该模型运行的环境只能是x86\_64架构的linux操作系统。
- 若转换后的离线模型不包含操作系统类型、架构，例如：_xxx_.om，则说明CANN软件包所支持的操作系统，都支持该模型运行。

**产品支持情况：**

全量芯片支持。

## SOC\_VERSION

图编译时使用的AI处理器型号。此参数实际对应的options参数为`ge.socVersion`。

- 若当前环境存在AI处理器，该参数可选。
- 若当前环境不存在AI处理器，即开发环境，该参数必填。

**参数取值：**

<soc\_version\>查询方法为：

<!-- npu="910b,910,310p,310b" id29 -->
- 针对如下产品：在安装AI处理器的服务器执行**npu-smi info**命令进行查询，获取**Name**信息。实际配置值为AscendName，例如**Name**取值为xxxyy，实际配置值为Ascendxxxyy。

    <!-- npu="910b" id30 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id30 -->

    <!-- npu="310b" id31 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id31 -->

    <!-- npu="310p" id32 -->
    Atlas 推理系列产品
    <!-- end id32 -->

    <!-- npu="910" id33 -->
    Atlas 训练系列产品
    <!-- end id33 -->
<!-- end id29 -->

<!-- npu="A3" id34 -->
- 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_** -c **_chip\_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascendxxx，**NPU Name**取值为1234，实际配置值为Ascendxxx\_1234。其中：
  - id：设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
  - chip\_id：芯片id，通过**npu-smi info -m**命令查出的Chip ID即为芯片id。
<!-- end id34 -->

<!-- npu="950" id35 -->
- 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascendxxx，**NPU Name**取值为1234，实际配置值为Ascendxxx\_1234。

    其中，id为设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
<!-- end id35 -->

<!-- npu="IPV350" id36 -->
IPV350参数值：Ascend035
<!-- end id36 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/basic_functions_res.md#id5 -->

**配置示例：**

```c++
{ge::ir_option::SOC_VERSION, "<soc_version>"}
```

**产品支持情况：**

全量芯片支持。

<!-- npu="950,A3,910b,910,310p,310b" id44 -->
## VIRTUAL\_TYPE

是否支持离线模型在昇腾虚拟化实例特性生成的虚拟设备上运行。此参数实际对应的options参数为`ge.virtual_type`。

当前芯片算力比较大，云端用户或者小企业完全不需要使用这么大算力，昇腾虚拟化实例特性支持对芯片的算力进行切分，可满足用户根据自己的业务按需申请算力的诉求。

虚拟设备是指按照指定算力在芯片上申请的虚拟加速资源。

**参数取值：**

- 0：（默认值）离线模型不在昇腾虚拟化实例特性生成的虚拟设备上运行。
- 1：离线模型在不同算力的虚拟设备上运行。

**配置示例：**

```c++
{ge::ir_option::VIRTUAL_TYPE, "1"}
```

**使用约束：**

1. 参数取值为1时，进行模型转换，则转换后离线模型参与计算的逻辑AI Core核数可能比实际aicore\_num核数大，为aicore\_num支持配置范围的最小公倍数：

    例如aicore\_num支持配置范围为\{1,2,4,8\}，参数取值为1转换后的离线模型，NPU运行核数可能为8。

2. 参数取值为1时，转换后的模型，如果包括如下算子，会默认使用单核，该场景下，将会导致转换后的模型推理性能下降。
    - DynamicRNN
    - PadV2D
    - SquareSumV2
    - DynamicRNNV2
    - DynamicRNNV3
    - DynamicGRUV

**产品支持情况：**

<!-- npu="950" id37 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id37 -->
<!-- npu="A3" id38 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id38 -->
<!-- npu="910b" id39 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id39 -->
<!-- npu="310b" id40 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id40 -->
<!-- npu="310p" id41 -->
- Atlas 推理系列产品：支持
<!-- end id41 -->
<!-- npu="910" id42 -->
- Atlas 训练系列产品：支持
<!-- end id42 -->
<!-- npu="IPV350" id43 -->
- IPV350：不支持
<!-- end id43 -->

<!-- end id44 -->
