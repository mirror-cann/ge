# 开发前准备

## 环境准备

支持的产品型号：Atlas A2 训练系列产品/Atlas A2 推理系列产品和Atlas A3 训练系列产品/Atlas A3 推理系列产品。

已经安装CANN软件，详细操作请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)。

> [!NOTE]说明
>AI Server场景下，安装节点应采用容器/虚拟机隔离，容器/虚拟机的生命周期与业务进程/租户保持一致。容器/虚拟机生命周期结束时要清理持久化数据，避免对下一个业务进程/租户的影响。

- 必选环境变量

    安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行`source ${INSTALL_DIR}/set_env.sh`命令设置环境变量。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

    ```shell
    export RESOURCE_CONFIG_PATH=numa_config.json //用于设置配置异构资源描述信息文件的存储路径。
    ```

    > [!NOTE]说明
    >numa\_config.json请参考[附录](appendices.md)。

- 可选环境变量

    ```shell
    export ASCEND_GLOBAL_LOG_LEVEL=0 //设置应用类日志的日志级别及各模块日志级别，仅支持调试日志。0对应DEBUG级别。
    export ASCEND_SLOG_PRINT_TO_STDOUT=1 //是否开启日志打印。1表示开启日志打印。
    ```

    具体说明请参考[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

上述环境变量只在当前窗口生效，用户可以将上述命令写入\~/.bashrc文件，使其永久生效，方法如下：

1. 以安装用户在任意目录下执行**vi \~/.bashrc**，在该文件最后添加上述内容。
2. 执行**:wq!**命令保存文件并退出。
3. 执行**source \~/.bashrc**使环境变量生效。

## 网络分析

通过FlowData和FlowNode构建Graph，需要根据网络，明确如下信息：

1. 网络包含几个输入，通过FlowData表示。
2. 网络包含几个计算节点，通过FlowNode表示。
3. FlowNode里的实际计算由ProcessPoint表示，ProcessPoint有两种，GraphPp和FunctionPp，如果是GraphPp请参考[《图开发》](https://hiascend.com/document/redirect/CannCommunityGraphguide)进行AscendGraph开发。如果是FunctionPp参考[专题](special_topics.md)中的“UDF开发”自定义用户功能。
