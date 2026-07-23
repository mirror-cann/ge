# --soc\_version

## 产品支持情况

全量芯片支持

## 功能说明

配置模型在推理运行阶段所使用的AI处理器型号。

模型转换阶段不依赖AI处理器，但是转换时使用的--soc\_version取值，必须为转换后模型运行阶段所使用的AI处理器型号。

兼容性说明：

- **针对8.5.0及之后版本**：使用ATC工具进行模型转换时，必须安装与目标AI处理器匹配的ops算子包，否则会导致编译失败。
- 不同产品，只要--soc\_version取值相同，则只需转换一次模型，即可分别在这些产品上进行部署运行。
- 部署运行环境要求：请确保产品运行环境的CANN软件包版本不低于转换om离线模型时转换环境的CANN软件包版本。
- 低版本的CANN软件包环境上转换出的om离线模型，支持在高版本的CANN软件包环境上运行，兼容4个版本周期。

## 关联参数

无。

<!-- npu="950,A3,910b,910,310p,310b" id5 -->
## 参数取值

参数值查询方法如下：

<!-- npu="910b,910,310p,310b" id6 -->
- 针对如下产品：在安装AI处理器的服务器执行**npu-smi info**命令进行查询，获取**Name**信息。实际配置值为AscendName，例如**Name**取值为*xxxyy*，实际配置值为Ascendxxxyy。

    <!-- npu="910b" id1 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id1 -->

    <!-- npu="310b" id2 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id2 -->

    <!-- npu="310p" id3 -->
    Atlas 推理系列产品
    <!-- end id3 -->

    <!-- npu="910" id4 -->
    Atlas 训练系列产品
    <!-- end id4 -->
<!-- end id6 -->

<!-- npu="A3" id8 -->
- 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行**npu-smi info -t board -i ***id*** -c **_chip\_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascendxxx，**NPU Name**取值为1234，实际配置值为Ascendxxx\_1234。其中：

  - id：设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。

  - chip\_id：芯片id，通过**npu-smi info -m**命令查出的Chip ID即为芯片id。
<!-- end id8 -->

<!-- npu="950" id9 -->
- 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascendxxx，**NPU Name**取值为1234，实际配置值为Ascendxxx\_1234。

    其中，id为设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
<!-- end id9 -->

<!-- end id5 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--soc_version_res.md#id1 -->

<!-- npu="IPV350" id10 -->
## 参数取值

Ascend035
<!-- end id10 -->

## 推荐配置及收益

无。

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--soc_version_res.md#id2 -->

<!-- npu="IPV350" id7 -->
## 示例

IPV350使用示例：

```bash
atc --soc_version=Ascend035 ...
```
<!-- end id7 -->

## 依赖约束

无。
