# aclmdlConfigAttr

## 定义

```c
typedef enum {
    ACL_MDL_PRIORITY_INT32 = 0,
    ACL_MDL_LOAD_TYPE_SIZET,
    ACL_MDL_PATH_PTR,
    ACL_MDL_MEM_ADDR_PTR,
    ACL_MDL_MEM_SIZET,
    ACL_MDL_WEIGHT_ADDR_PTR,
    ACL_MDL_WEIGHT_SIZET,
    ACL_MDL_WORKSPACE_ADDR_PTR,
    ACL_MDL_WORKSPACE_SIZET,
    ACL_MDL_INPUTQ_NUM_SIZET,
    ACL_MDL_INPUTQ_ADDR_PTR,
    ACL_MDL_OUTPUTQ_NUM_SIZET,
    ACL_MDL_OUTPUTQ_ADDR_PTR,
    ACL_MDL_WORKSPACE_MEM_OPTIMIZE,
    ACL_MDL_WEIGHT_PATH_PTR,
    ACL_MDL_MODEL_DESC_PTR,
    ACL_MDL_MODEL_DESC_SIZET,
    ACL_MDL_KERNEL_PTR,
    ACL_MDL_KERNEL_SIZET,
    ACL_MDL_KERNEL_ARGS_PTR,
    ACL_MDL_KERNEL_ARGS_SIZET,
    ACL_MDL_STATIC_TASK_PTR,
    ACL_MDL_STATIC_TASK_SIZET,
    ACL_MDL_DYNAMIC_TASK_PTR,
    ACL_MDL_DYNAMIC_TASK_SIZET,
    ACL_MDL_MEM_MALLOC_POLICY_SIZET,
    ACL_MDL_FIFO_PTR,
    ACL_MDL_FIFO_SIZET,
    ACL_MDL_WITHOUT_GRAPH_INT32
} aclmdlConfigAttr;
```

## ACL\_MDL\_PRIORITY\_INT32取值说明

模型执行的优先级，可选项。该选项对应的值为int32类型。

数字越小优先级越高，取值[0,7]，默认值为0。

<!-- npu="910" id1 -->
Atlas 训练系列产品，docker容器内安装CANN，且配置算力分组的场景，该枚举值不生效。
<!-- end id1 -->

## ACL\_MDL\_LOAD\_TYPE\_SIZET取值说明

模型加载方式，必选项。该选项对应的值为size\_t类型。

ACL\_MDL\_LOAD\_TYPE\_SIZET（表示模型加载方式）的取值如下：

- ACL\_MDL\_LOAD\_FROM\_FILE

    ```c
    #define ACL_MDL_LOAD_FROM_FILE 1
    ```

- ACL\_MDL\_LOAD\_FROM\_FILE\_WITH\_MEM

    ```c
    #define ACL_MDL_LOAD_FROM_FILE_WITH_MEM 2
    ```

- ACL\_MDL\_LOAD\_FROM\_MEM

    ```c
    #define ACL_MDL_LOAD_FROM_MEM 3
    ```

- ACL\_MDL\_LOAD\_FROM\_MEM\_WITH\_MEM

    ```c
    #define ACL_MDL_LOAD_FROM_MEM_WITH_MEM 4
    ```

- ACL\_MDL\_LOAD\_FROM\_FILE\_WITH\_Q

    ```c
    #define ACL_MDL_LOAD_FROM_FILE_WITH_Q 5
    ```

- ACL\_MDL\_LOAD\_FROM\_MEM\_WITH\_Q

    ```c
    #define ACL_MDL_LOAD_FROM_MEM_WITH_Q 6
    ```

**注意**：如果将ACL\_MDL\_LOAD\_TYPE\_SIZET设置为ACL\_MDL\_LOAD\_FROM\_MEM，表示从内存加载模型数据，还支持使用ACL\_MDL\_WEIGHT\_PATH\_PTR选项指定权重文件目录。

## ACL\_MDL\_PATH\_PTR取值说明

om模型文件路径的指针，如果选择从文件加载模型，则该选项必选。关于如何获取模型文件，请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“参数说明 \> 基础功能参数 \> 总体选项 \> --mode”。

<!-- npu="IPV350" id2 -->
IPV350上还支持后缀为exeom的模型文件，关于om或exeom模型文件的区别如下：

- \*.om文件不感知具体的硬件调度能力、包含中间态的抽象数据结构，在模型加载阶段，再根据具体执行平台的调度特性，生成运行时数据结构。
- \*.exeom文件感知具体的硬件调度能力、包含目标执行平台的运行时数据结构（这些数据以二进制的形式保存在\*.exeom文件中），在模型加载阶段，加载恢复二进制内容，根据用户应用程序传递的数据区地址，或实际申请到的数据地址，刷新二进制中的地址指针值后，将二进制内容直接拷贝至Device，达到提升模型加载性能、降低模型加载内存峰值占用的效果。**在一些资源受限的场景，建议使用\*.exeom模型文件，增强产品的商用竞争力。**
<!-- end id2 -->

## ACL\_MDL\_MEM\_ADDR\_PTR取值说明

模型在内存中的起始地址，如果选择从内存加载模型，则该选项必选。

## ACL\_MDL\_MEM\_SIZET取值说明

模型在内存中的大小，如果选择从内存加载模型，则该选项必选，与ACL\_MDL\_MEM\_ADDR\_PTR选项配合使用。该选项对应的值为size\_t类型。

## ACL\_MDL\_WEIGHT\_ADDR\_PTR取值说明

Device上模型权值内存（存放权值数据）的指针，如果需要由用户管理权值内存，则该选项必选。若不配置该选项，则表示由系统管理内存。

## ACL\_MDL\_WEIGHT\_SIZET取值说明

权值内存大小，单位为Byte，如果需要由用户管理权值内存，则该选项必选，与ACL\_MDL\_WEIGHT\_ADDR\_PTR选项配合使用。该选项对应的值为size\_t类型。

## ACL\_MDL\_WORKSPACE\_ADDR\_PTR取值说明

Device上模型所需工作内存（存放模型执行过程中的临时数据）的指针，如果需要由用户管理工作内存，则该选项必选。若不配置该选项，则表示由系统管理内存。

<!-- npu="IPV350" id3 -->
当前版本不支持该配置。
<!-- end id3 -->

## ACL\_MDL\_WORKSPACE\_SIZET取值说明

模型所需工作内存的大小，单位为Byte，如果需要由用户管理工作内存，则该选项必选，与ACL\_MDL\_WORKSPACE\_ADDR\_PTR选项配合使用。该选项对应的值为size\_t类型。

<!-- npu="IPV350" id4 -->
当前版本不支持该配置。
<!-- end id4 -->

## ACL\_MDL\_INPUTQ\_NUM\_SIZET取值说明

模型输入队列大小 ,带队列加载模型时，该选项必选，与ACL\_MDL\_INPUTQ\_ADDR\_PTR选项配合使用。该选项对应的值为size\_t类型。

<!-- npu="IPV350" id5 -->
当前版本不支持该配置。
<!-- end id5 -->

## ACL\_MDL\_INPUTQ\_ADDR\_PTR取值说明

模型输入队列ID的指针，带队列加载模型时，该选项必选，一个模型输入对应一个队列ID。

<!-- npu="IPV350" id6 -->
当前版本不支持该配置。
<!-- end id6 -->

## ACL\_MDL\_OUTPUTQ\_NUM\_SIZET取值说明

模型输出队列大小，带队列加载模型时，该选项必选，与ACL\_MDL\_OUTPUTQ\_ADDR\_PTR选项配合使用。该选项对应的值为size\_t类型。

<!-- npu="IPV350" id7 -->
当前版本不支持该配置。
<!-- end id7 -->

## ACL\_MDL\_OUTPUTQ\_ADDR\_PTR取值说明

模型输出队列ID的指针，带队列加载模型时，该选项必选，一个模型输出对应一个队列ID。

<!-- npu="IPV350" id8 -->
当前版本不支持该配置。
<!-- end id8 -->

## ACL\_MDL\_WORKSPACE\_MEM\_OPTIMIZE取值说明

是否开启模型工作内存优化功能，1开启，0不开启。若关注内存规划或内存资源有限时，建议选择由系统管理内存的方式加载模型，并开启工作内存优化功能，此时工作内存中不包含存放模型输入、输出数据的内存，工作内存大小会减小，达到节省内存的目的。在模型执行前，还需要由用户申请存放模型输入、输出数据的内存，因此即使在模型加载时开启工作内存优化功能，也不会影响后续的模型执行。

<!-- npu="IPV350" id9 -->
当前版本不支持该配置。
<!-- end id9 -->

## ACL\_MDL\_WEIGHT\_PATH\_PTR取值说明

权重文件所在目录的指针。对om模型文件大小有限制的场景下，通过本参数可实现权重文件外置功能。

如果将ACL\_MDL\_LOAD\_TYPE\_SIZET设置为ACL\_MDL\_LOAD\_FROM\_MEM，表示从内存加载模型数据，则支持使用ACL\_MDL\_WEIGHT\_PATH\_PTR指定权重文件目录。

一般对om模型文件大小有限制或模型文件加密的场景下，需单独加载权重文件，因此需在构建模型时，将权重保存在单独的文件中。例如在使用ATC工具生成om文件时，将--external\_weight参数设置为1（1表示将原始网络中的Const/Constant节点的权重保存在单独的文件中）。

**注意：**通过ACL\_MDL\_WEIGHT\_PATH\_PTR配置权重文件所在目录的指针后，在模型加载过程中，图引擎（Graph Engine，简称GE）将自行申请一块Device内存，读取外置权重文件，并将外置权重数据拷贝至该Device内存中，模型卸载时会释放该内存。若已调用[aclmdlSetExternalWeightAddress](aclmdlSetExternalWeightAddress.md)接口配置存放外置权重的Device内存，由用户自行管理Device内存，则无需配置ACL\_MDL\_WEIGHT\_PATH\_PTR；若针对同一个模型，既配置了ACL\_MDL\_WEIGHT\_PATH\_PTR，也调用了[aclmdlSetExternalWeightAddress](aclmdlSetExternalWeightAddress.md)接口，则[aclmdlSetExternalWeightAddress](aclmdlSetExternalWeightAddress.md)接口的优先级更高。

## ACL\_MDL\_MODEL\_DESC\_PTR取值说明

存放模型描述信息的内存指针。

<!-- npu="950,A3,910b,910,310p,310b" id10 -->
当前版本不支持该配置。
<!-- end id10 -->

## ACL\_MDL\_MODEL\_DESC\_SIZET取值说明

存放模型描述信息所需的内存大小，单位Byte。该选项对应的值为size\_t类型。

可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取存放模型描述信息所需的内存大小，且本选项需与ACL\_MDL\_MODEL\_DESC\_PTR选项配合使用。

<!-- npu="950,A3,910b,910,310p,310b" id11 -->
当前版本不支持该配置。
<!-- end id11 -->

## ACL\_MDL\_KERNEL\_PTR取值说明

存放TBE算子kernel（算子的\*.o与\*.json）的内存指针。

<!-- npu="950,A3,910b,910,310p,310b" id12 -->
当前版本不支持该配置。
<!-- end id12 -->

## ACL\_MDL\_KERNEL\_SIZET取值说明

存放TBE算子kernel（算子的\*.o与\*.json）所需的内存大小，单位Byte。该选项对应的值为size\_t类型。

可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取存放TBE算子kernel（算子的\*.o与\*.json）所需的内存大小，且本选项需与ACL\_MDL\_KERNEL\_PTR选项配合使用。

<!-- npu="950,A3,910b,910,310p,310b" id13 -->
当前版本不支持该配置。
<!-- end id13 -->

## ACL\_MDL\_KERNEL\_ARGS\_PTR取值说明

存放TBE算子kernel参数的内存指针。

<!-- npu="950,A3,910b,910,310p,310b" id14 -->
当前版本不支持该配置。
<!-- end id14 -->

## ACL\_MDL\_KERNEL\_ARGS\_SIZET取值说明

存放TBE算子kernel参数所需的内存大小，单位Byte。该选项对应的值为size\_t类型。

可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取存放TBE算子kernel参数所需的内存大小，且本选项需与ACL\_MDL\_KERNEL\_ARGS\_PTR选项配合使用。

<!-- npu="950,A3,910b,910,310p,310b" id15 -->
当前版本不支持该配置。
<!-- end id15 -->

## ACL\_MDL\_STATIC\_TASK\_PTR取值说明

存放静态shape任务描述信息的内存指针。

<!-- npu="950,A3,910b,910,310p,310b" id16 -->
当前版本不支持该配置。
<!-- end id16 -->

## ACL\_MDL\_STATIC\_TASK\_SIZET取值说明

存放静态shape任务描述信息所需的内存大小，单位Byte。该选项对应的值为size\_t类型。

可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取存放静态shape任务描述信息所需的内存大小，且本选项需与ACL\_MDL\_STATIC\_TASK\_PTR选项配合使用。

<!-- npu="950,A3,910b,910,310p,310b" id17 -->
当前版本不支持该配置。
<!-- end id17 -->

## ACL\_MDL\_DYNAMIC\_TASK\_PTR取值说明

存放动态shape任务描述信息的内存指针。

<!-- npu="950,A3,910b,910,310p,310b" id18 -->
当前版本不支持该配置。
<!-- end id18 -->

## ACL\_MDL\_DYNAMIC\_TASK\_SIZET取值说明

存放动态shape任务描述信息所需的内存大小，单位Byte。该选项对应的值为size\_t类型。

可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取存放动态shape任务描述信息所需的内存大小，且本选项需与ACL\_MDL\_DYNAMIC\_TASK\_PTR选项配合使用。

<!-- npu="950,A3,910b,910,310p,310b" id19 -->
当前版本不支持该配置。
<!-- end id19 -->

## ACL\_MDL\_MEM\_MALLOC\_POLICY\_SIZET取值说明

内存分配规则，该选项对应的值为size\_t类型。

<!-- npu="950,A3,910b,910,310p,310b" id20 -->
当前版本不支持该配置。
<!-- end id20 -->

<!-- npu="IPV350" id21 -->
**支持如下取值：**

- ACL\_MEM\_MALLOC\_HUGE\_FIRST

    当申请的内存小于等于1M时，即使使用该内存分配规则，也是申请普通页的内存。当申请的内存大于1M时，优先申请大页内存，如果大页内存不够，则使用普通页的内存。

- ACL\_MEM\_MALLOC\_HUGE\_ONLY

    配置该选项时，表示仅申请大页，如果大页内存不够，则返回错误。

- ACL\_MEM\_MALLOC\_NORMAL\_ONLY

    仅申请普通页，如果普通页内存不够，则返回错误。

- ACL\_MEM\_TYPE\_LOW\_BAND\_WIDTH

    从带宽低的物理内存上申请内存。

- ACL\_MEM\_TYPE\_HIGH\_BAND\_WIDTH

    从带宽高的物理内存上申请内存。

**此处支持单个取值，也支持多个取值位或：**

- **配置单个取值**：

    若配置ACL\_MEM\_MALLOC\_HUGE\_FIRST、ACL\_MEM\_MALLOC\_HUGE\_ONLY、ACL\_MEM\_MALLOC\_NORMAL\_ONLY中的其中一个，则系统内部会根据硬件支持情况选择从高带宽或低带宽物理内存申请内存；

    若配置ACL\_MEM\_TYPE\_LOW\_BAND\_WIDTH或ACL\_MEM\_TYPE\_HIGH\_BAND\_WIDTH，则系统内部会默认采取ACL\_MEM\_MALLOC\_HUGE\_FIRST，优先申请大页。

- **配置多个取值位或**：支持这三项（ACL\_MEM\_MALLOC\_HUGE\_FIRST、ACL\_MEM\_MALLOC\_HUGE\_ONLY、ACL\_MEM\_MALLOC\_NORMAL\_ONLY）与这两项（ACL\_MEM\_TYPE\_LOW\_BAND\_WIDTH、ACL\_MEM\_TYPE\_HIGH\_BAND\_WIDTH）组合，**例如**：ACL\_MEM\_MALLOC\_HUGE\_FIRST | ACL\_MEM\_TYPE\_HIGH\_BAND\_WIDTH
<!-- end id21 -->

## ACL\_MDL\_FIFO\_PTR取值说明

模型级别全局内存的起始地址。此处是指Device上的内存。

<!-- npu="950,A3,910b,910,310p,310b" id22 -->
当前版本不支持该配置。
<!-- end id22 -->

<!-- npu="IPV350" id23 -->
若某个模型在推理时，其每一层的输入来自上一层的输出以及前面几轮推理结果拼接而成时，则需使用模型级别的全局内存将该模型所需的输入数据保存下来，供后续推理使用。

**注意**：对于同一个模型加载一次并行执行多次推理的场景，此时可能存在多个推理任务同时访问模型级别全局内存的情况，导致推理结果异常。建议加载一次模型后，串行执行多次推理。
<!-- end id23 -->

## ACL\_MDL\_FIFO\_SIZET取值说明

模型级别全局内存的大小，该选项对应的值为size\_t类型。

<!-- npu="950,A3,910b,910,310p,310b" id24 -->
当前版本不支持该配置。
<!-- end id24 -->

<!-- npu="IPV350" id25 -->
可提前调用[aclmdlQueryExeOMDesc](aclmdlQueryExeOMDesc.md)接口获取模型级别全局内存的大小。
<!-- end id25 -->

## ACL\_MDL\_WITHOUT\_GRAPH\_INT32取值说明

模型加载后是否保留计算图及其预缓存信息，只支持输入为静态Shape的场景。该选项对应的值为INT32类型，取值范围如下：

- 0：默认值，保留计算图及其预缓存信息。
- 1：不保留计算图及其预缓存信息。设置为1时，模型加载成功后会释放计算图及其预缓存信息，用于节省内存，但预缓存信息被释放，会影响Profiling数据采集以及异常场景下的算子信息获取，因此：
    - Profiling数据采集场景下，需在模型加载前调用aclprofStart接口打开Profling开关，否则Profiling功能不可用。
    - 异常场景下，调用[aclmdlCreateAndGetOpDesc](aclmdlCreateAndGetOpDesc.md)接口获取算子描述信息时会失败。
