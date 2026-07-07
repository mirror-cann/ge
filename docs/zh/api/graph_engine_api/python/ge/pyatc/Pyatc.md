# Pyatc接口

## 产品支持情况

全量芯片支持。

## 功能说明

pyatc是ATC（Ascend Tensor Compiler）离线模型编译工具的Python封装接口，在功能上与ATC命令行工具完全等价，旨在为Python用户提供更灵活的集成体验。

pyatc与ATC命令行工具的区别是：

- ATC命令行工具：作为独立子进程启动，编译过程所使用的Python解释器由atc进程自行解析，可能与用户当前终端环境（如python3）不一致，容易引发依赖冲突或路径问题。
- pyatc：直接在当前主进程中执行编译逻辑，复用用户当前的Python解释器及环境变量，确保编译环境与运行环境完全一致，避免了进程间的环境隔离问题。

## 调用方式

先参见[环境准备](../../../../../user_guides/graph_dev/overview/environment_setup.md)完成相关环境变量的设置，然后根据实际使用场景选择以下任一入口：

- 使用命令行入口：

    ```bash
    pyatc [参数]
    ```

- 使用Python模块入口：

    ```bash
    python3 -m ge.pyatc [参数]
    ```

## 参数说明

pyatc的参数与ATC命令行工具完全一致，详细功能介绍请参见《ATC离线模型编译工具》。

## 返回值说明

调用结束后通过进程退出码（exit code）表示执行结果：

- 0：执行成功。
- 非 0：执行失败，错误信息输出至标准输出。

## 调用示例

```bash
pyatc --model=resnet50.onnx --framework=5 --soc_version=<soc_version> --output=resnet50
```

```bash
pyatc --model=resnet50.onnx --framework=5 --soc_version=MC62CM12A* --output=resnet50
```

```bash
pyatc --model=resnet50.onnx --framework=5 --soc_version=Ascend035 --output=resnet50
```

_<soc\_version\>_取值或者查询方法如下：

- 针对如下产品：在安装AI处理器的服务器执行**npu-smi info**命令进行查询，获取**Name**信息。实际配置值为AscendName，例如**Name**取值为_xxxyy_，实际配置值为Ascend_xxxyy_。

    Atlas A2 训练系列产品/Atlas A2 推理系列产品

    Atlas 200I/500 A2 推理产品

    Atlas 推理系列产品

    Atlas 训练系列产品

- 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_** -c **_chip\_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascend_xxx_，**NPU Name**取值为1234，实际配置值为Ascend_xxx__\__1234。其中：
   - id：设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
   - chip\_id：芯片id，通过**npu-smi info -m**命令查出的Chip ID即为芯片id。

- 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行**npu-smi info -t board -i **_id_命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为Ascend_xxx_，**NPU Name**取值为1234，实际配置值为Ascend_xxx__\__1234。

    其中，id为设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。

BS9SX1A AI处理器参数值：BS9SX1A\*

BS9SX2A AI处理器参数值：BS9SX2A\*

其中：\*可能根据芯片性能提升等级、芯片核数使用等级等因素会有不同的取值，例如取值可以为A、B或C，请根据实际情况获取对应的取值。
