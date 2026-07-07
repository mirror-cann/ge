# 属性名列表

本节所列属性名为通过[SetAttr](./GNode/SetAttr.md)接口可设置的属性名称，级别都为算子级，头文件位于CANN软件安装后文件存储路径下的include/ge/ge\_api\_types.h。

## \_enable\_inner\_parallel

是否允许\_user\_stream\_label内的算子按照GE原有的并发策略分流。

**参数取值：**

- true：\_user\_stream\_label范围内的算子按照GE原有的并发策略分流。
- false：\_user\_stream\_label范围内的所有算子将不再区分原有并发策略，统一合并到同一计算流中执行。

**使用约束：**

该属性需要配合\_user\_stream\_label使用，当\_enable\_inner\_parallel属性为true时，\_user\_stream\_label范围内的算子还按照GE原有的并发策略分流（如CMO算子单独分流）

**产品支持情况：**

全量芯片支持。

## \_op\_aicore\_num

用于配置算子编译时使用的AI Core中的Cube Core核数。

参数取值：整数类型，整数需要大于0，小于等于AI处理器包含的最大Cube Core。

不同AI处理器包含的最大CubeCore的数量可从"$\{INSTALL\_DIR\}/_<arch\>_-linux/data/platform\_config/_xxx_.ini"文件查看，如下所示，说明AI处理器上存在24个Cube Core。

```ini
[SoCInfo]
# 参数配置为默认值，默认值即为最大值
ai_core_cnt=24
cube_core_cnt=24
vector_core_cnt=48
```

其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。_<arch\>_表示具体操作系统架构，_xxx_请根据实际产品进行选择。

**产品支持情况：**

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持
- Atlas 200I/500 A2 推理产品：不支持
- MC62CM12A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- IPV350：不支持

## \_op\_vectorcore\_num

用于配置算子编译时使用的AI Core中的Vector Core核数。

参数取值：整数类型，整数需要大于0，小于等于AI处理器包含的最大Vector Core。

不同AI处理器包含的最大VectorCore的数量可从"$\{INSTALL\_DIR\}/_<arch\>_-linux/data/platform\_config/_xxx_.ini"文件查看，如下所示，说明AI处理器上存在48个Vector Core。

```ini
[SoCInfo]
# 参数配置为默认值，默认值即为最大值
ai_core_cnt=24
cube_core_cnt=24
vector_core_cnt=48
```

其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。_<arch\>_表示具体操作系统架构，_xxx_请根据实际产品进行选择。

**产品支持情况：**

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 推理系列产品：不支持
- Atlas 训练系列产品：不支持
- Atlas 200I/500 A2 推理产品：不支持
- MC62CM12A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- IPV350：不支持

## \_op\_exec\_never\_timeout

设置静态图上的AI Core算子执行时是否受超时时间限制。

**参数取值：**

- true：算子执行不受超时时间限制，永不超时。
- false：算子执行受超时时间限制。

用户可以通过《Runtime运行时 API》中的“执行控制 \> aclrtSetOpExecuteTimeOutV2”接口设置算子执行的超时时间。

**使用约束：**

如果AI Core算子开启了Tiling下沉特性，即开启[性能调优](./options_params/precision_tuning.md)中的ge.tiling\_schedule\_optimize参数，则此类算子不支持设置永不超时属性。

**产品支持情况：**

全量芯片支持。

## \_user\_stream\_label

字符串类型，指定算子执行的目标stream标签（需要切换到的流的标签），相同的标签代表相同的流，由用户控制。

图执行过程中如需开启“**图内多流表达功能**”，可通过\_user\_stream\_label和\_user\_stream\_priority指定图内多个算子分发到不同stream做并行计算，提高资源利用率。

**产品支持情况：**

全量芯片支持。

## \_user\_stream\_priority

int类型，表示切换到\_user\_stream\_label流的优先级，即Runtime运行时在并发时优先给高优先级的流分配核资源。当前版本为预留参数，建议取默认值0。

图执行过程中如需开启“**图内多流表达功能**”，可通过\_user\_stream\_label和\_user\_stream\_priority指定图内多个算子分发到不同stream做并行计算，提高资源利用率。

**产品支持情况：**

全量芯片支持。
