# --aicore\_num

## 产品支持情况

全量芯片支持。

## 功能说明

用于配置模型编译时使用的AI Core核数。

## 参数取值

**参数值：**"整数1|整数2"，中间使用“|”分隔。

<!-- npu="950,A3,910b" id11 -->
- **场景1**：针对如下产品，整数1表示算子编译时使用的AI Core中的Cube Core核数，整数2表示算子编译时使用的AI Core中的Vector Core核数，整数1与整数2都需要大于0，小于等于AI处理器包含的最大Cube Core和Vector Core数量：

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT
    <!-- end id1 -->

    <!-- npu="A3" id2 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id2 -->

    <!-- npu="910b" id3 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id3 -->
<!-- end id11 -->

<!-- npu="910,310p,310b,IPV350" id12 -->
- **场景2**：针对如下产品，仅需配置整数1，配置格式为："整数1|"，配置整数2不会生效，表示算子编译时使用的AI Core核数：

    <!-- npu="310b" id4 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id4 -->

    <!-- npu="310p" id5 -->
    Atlas 推理系列产品
    <!-- end id5 -->

    <!-- npu="910" id6 -->
    Atlas 训练系列产品
    <!-- end id6 -->

    <!-- npu="IPV350" id7 -->
    IPV350
    <!-- end id7 -->
<!-- end id12 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--aicore_num_res.md#id1 -->

**参数值约束**：

<!-- npu="950,A3,910b" id13 -->
- 针对参数值中的场景1：

    不同AI处理器包含的最大Cube Core与Vector Core的数量可从"$\{INSTALL\_DIR\}/_<arch\>_-linux/data/platform\_config/_xxx_.ini"文件查看，如下所示，说明AI处理器上存在24个Cube Core，存在48个Vector Core。

    ```ini
    [SoCInfo]
    # 参数配置为默认值，默认值即为最大值
    ai_core_cnt=24
    cube_core_cnt=24
    vector_core_cnt=48
    ```
<!-- end id13 -->

<!-- npu="910,310p,310b,IPV350" id14 -->
- 针对参数值中的场景2：

    不同AI处理器包含的最大AI Core数量可从"$\{INSTALL\_DIR\}/_<arch\>_-linux/data/platform\_config/_xxx_.ini"文件查看，如下所示，说明AI处理器上存在10个AI Core。

    ```ini
    [SoCInfo]
    # AI Core默认值，默认值即为最大值
    ai_core_cnt=10
    vector_core_cnt=8
    ```
<!-- end id14 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--aicore_num_res.md#id2 -->

- 如果配置该参数的同时启用了算子编译缓存功能（[--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)参数配置为“enable”或者“force”），此参数仅在首次编译时生效。若您想在非首次编译时生效该参数，需要清理编译磁盘的缓存。

其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。<arch\>表示具体操作系统架构，xxx请根据实际产品进行选择。

## 推荐配置及收益

无。

## 示例

<!-- npu="950,A3,910b" id15 -->
- 场景1配置示例：

    ```bash
    atc --aicore_num="24|48" ...
    ```
<!-- end id15 -->

<!-- npu="910,310p,310b,IPV350" id16 -->
- 场景2配置示例

    ```bash
    atc --aicore_num="10|" ...
    或
    atc --aicore_num="10" ...
    ```
<!-- end id16 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--aicore_num_res.md#id3 -->

## 了解AI Core、Cube Core、Vector Core的关系

为便于理解AI Core、Cube Core、Vector Core的关系，此处先明确Core的定义，Core是指拥有独立Scalar计算单元的一个计算核，通常Scalar计算单元承担了一个计算核的SIMD（单指令多数据，Single Instruction Multiple Data）指令发射等功能，所以我们也通常也把这个Scalar计算单元称为核内的调度单元。不同产品上的AI数据处理核心单元不同，当前分为以下几类：

- 当AI数据处理核心单元是AI Core：
  - 在AI Core内，Cube和Vector共用一个Scalar调度单元。

    <!-- npu="910" id8 -->
    此处以Atlas 训练系列产品为例。
    <!-- end id8 -->

    ![图示](../figures/logical_architecture.png)

  - 在AI Core内，Cube和Vector都有各自的Scalar调度单元，因此又被称为Cube Core、Vector Core。这时，一个Cube Core和一组Vector Core被定义为一个AI Core，AI Core数量通常是以多少个Cube Core为基准计算的。

    <!-- npu="910b" id9 -->
    此处以Atlas A2 训练系列产品/Atlas A2 推理系列产品为例。
    <!-- end id9 -->

    ![图示](../figures/logical_architecture_0.png)

- 当AI数据处理核心单元是AI Core以及单独的Vector Core：AI Core和Vector Core都拥有独立的Scalar调度单元。

    <!-- npu="310p" id10 -->
    此处以Atlas 推理系列产品为例。
    <!-- end id10 -->

    ![图示](../figures/logical_architecture_1.png)

## 依赖约束

无。
