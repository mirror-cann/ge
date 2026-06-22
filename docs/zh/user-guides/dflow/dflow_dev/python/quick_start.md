# 快速入门

## 整体开发流程

![](figures/260109165520203.png)

## 环境准备

支持的产品型号：Atlas A2 训练系列产品/Atlas A2 推理系列产品和Atlas A3 训练系列产品/Atlas A3 推理系列产品。

- 当前仅支持Python3.11。安装方法请参考Python官网[https://www.python.org/](https://www.python.org/)。

- 已经安装好开发套件包Ascend-cann-toolkit，详细操作请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)。

    > [!NOTE]说明
    >AI Server场景下，安装节点应采用容器/虚拟机隔离，容器/虚拟机的生命周期与业务进程/租户保持一致。容器/虚拟机生命周期结束时要清理持久化数据，避免对下一个业务进程/租户的影响。

- 必选环境变量

    安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行**source \$_\{INSTALL\_DIR\}_/set\_env.sh**命令设置环境变量。\$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

    ```shell
    export RESOURCE_CONFIG_PATH=numa_config.json //用于设置配置异构资源描述信息文件的存储路径。
    ```

    > [!NOTE]说明
    >numa\_config.json请参考[numa\_config.json配置](appendices.md#numa_configjson配置)。

上述环境变量只在当前窗口生效，用户可以将上述命令写入\~/.bashrc文件，使其永久生效，方法如下：

1. 以安装用户在任意目录下执行**vi \~/.bashrc**，在该文件最后添加上述内容。
2. 执行**:wq!**命令保存文件并退出。
3. 执行**source \~/.bashrc**使环境变量生效。

## 完整样例参考

|Sample名称|代码地址|场景说明|
|--|--|--|
|sample_multiple_model|获取样例|多模型串接下沉执行。使用FuncProcessPoint和GraphProcessPoint构造DataFlow图，通过将GraphProcessPoint执行的onnx、pb模型下沉到device，减少host和device之间控制面、数据面交互，提高执行性能。|
|sample_pytorch|获取样例|sample_pytorch：DataFlow结合PyTorch做模型在线推理。将原来预处理、模型推理、后处理的串行执行流程改造成能够异步流水执行的FlowGraph，从而提升推理整体的吞吐量。|
|sample_npu_model|获取样例|针对sample_pytorch的优化，增加了对PyTorch模型节点的数据下沉或者模型下沉。|
