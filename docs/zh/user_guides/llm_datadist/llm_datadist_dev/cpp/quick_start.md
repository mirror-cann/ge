# 快速入门

## 整体开发流程

**简介**

为了方便后续的描述，我们对于不同的推理框架抽象为几个模块：

- 资源初始化模块。
- KV Cache管理模块。包括KV Cache的内存的创建，分配（PA场景）以及销毁。
- 模型推理模块。
- 资源释放模块。

本章节主要是介绍开发者如何在推理框架中使能LLM-DataDist的能力。

**开发流程**

1. 找到推理框架中的资源初始化模块，在该阶段中调用LLM-DataDist的初始化接口和建链接口。
2. 找到推理框架中的KV Cache管理模块，在该阶段中调用LLM-DataDist的KV Cache申请接口，将申请好的KV Cache转换为不同框架的KV Cache类型进行推理。
3. 推理框架需要拆分出Prefill阶段和Decode阶段，对推理脚本进行分离部署，部署到不同的集群节点上。在Decode阶段执行前需要接收来自Prefill阶段的输出作为输入，同时调用LLM-DataDist提供的KV Cache传输接口拉取或推送Prefill侧缓存的KV Cache。
4. 分别执行Prefill推理脚本和Decode推理脚本。
5. 在框架资源释放模块释放LLM-DataDist相关资源。

## 环境准备

支持的产品形态如下：
<!-- npu="910b" id1 -->

- Atlas A2 训练系列产品/Atlas A2 推理系列产品：针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，仅支持Atlas 800I A2 推理服务器、A200I A2 Box 异构组件。
<!-- end id1 -->

<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id2 -->

请参考[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)安装好驱动固件以及CANN软件。

使用hccn\_tool查询Device IP，并且进行卡间网络检测，要求各个集群上的卡间有RDMA链路连接，否则无法使能LLM-DataDist能力。hccn\_tool详细介绍请参考《[HCCN Tool 接口参考](https://support.huawei.com/enterprise/zh/ascend-computing/ascend-hdk-pid-252764743?category=developer-documents&subcategory=interface-reference)》。以下是常用命令参考。

|命令|使用场景|
|--|--|
|hccn_tool [-i %d] -link -g|获取指定Device网口Link状态。-i指定Device。样例：`hccn_tool -i 0 -link -g`|
|hccn_tool [-i %d] -ip -g|获取IP地址和子网掩码。-i指定Device。样例：`hccn_tool -i 0 -ip -g`|
|hccn_tool [-i %d] -ping -g [address %s ]|获取指定设备到目的地址的ping结果。-i指定当前server的某个Device， address指定ping的目的地址。样例：`hccn_tool -i 0 -ping -g address 192.168.2.1`|

使用LLM-DataDist过程中，还涉及到如下环境变量，具体请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

|名称|使用场景|
|--|--|
|HCCL_RDMA_TC、HCCL_RDMA_SL|当客户对参数面网络做了自己的规划时，对各种业务流量规定了类型，优先级。通过这两个环境变量设置参数面集合通信流量在网络上的流量类型和优先级，以适配客户网络流量规划的要求。|
|HCCL_RDMA_RETRY_CNT、HCCL_RDMA_TIMEOUT|分别对应RDMA网卡的重试次数和重传超时时间的系数timeout。设置太大导致对网络异常反应不敏感，不能感知到网络故障。设置太小则容易造成网络闪断直接造成业务中断，不能被网卡硬件屏蔽。用户可根据自身网络情况，来设置合适的值。例如，可以根据大部分闪断的时间范围进行配置。推荐按照如下公式进行配置，以减少网络抖动带来的影响。HCCL_RDMA_TIMEOUT=log2(pull kv超时时间 * 10^6 / (HCCL_RDMA_RETRY_CNT + 1) / 4.096)，向上取整。当pull kv超时时间和HCCL_RDMA_RETRY_CNT都等于默认值时，HCCL_RDMA_TIMEOUT建议配置成15。|
|HCCL_INTRA_ROCE_ENABLE|用于配置Server内是否使用RoCE环路进行多卡间的通信。|

## 完整样例参考

本示例通过LLM-DataDist接口实现分离部署场景下KV Cache管理功能。为不同框架使用LLM-DataDist的功能提供一些参考和改造思路。

快速体验：请单击[gitcode](https://gitcode.com/cann/ge)，选择对应分支，在“examples/dflow/cpp”目录中获取样例。
