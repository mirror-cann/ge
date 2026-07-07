# 功能调试

## DEBUG\_DIR

用于配置保存算子编译生成的调试相关的过程文件的路径，过程文件包括但不限于算子.o（算子二进制文件）、.json（算子描述文件）、.cce等文件。此参数实际对应的options参数为`ge.debugDir`。

默认生成在当前路径下。

**使用约束：**

- 如果要自行指定算子编译的过程文件存放路径，需DEBUG\_DIR参数与OP\_DEBUG\_LEVEL参数配合使用，且当OP\_DEBUG\_LEVEL取值为0时，不能使用DEBUG\_DIR参数。
- 算子编译生成的调试文件存储路径，除DEBUG\_DIR参数设置的方式外，还可以配置环境变量ASCEND\_WORK\_PATH，几种方式优先级为：配置参数“DEBUG\_DIR”\>环境变量ASCEND\_WORK\_PATH \>默认存储路径。关于环境变量ASCEND\_WORK\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

**配置示例**：

```c++
{ge::ir_option::OP_DEBUG_LEVEL, "1"},
{ge::ir_option::DEBUG_DIR, "/home/test/module/out_debug_info"}
```

**产品支持情况：**

全量芯片支持。

## OP\_DEBUG\_LEVEL

算子编译debug功能开关。此参数实际对应的options参数为`ge.opDebugLevel`。

如果要自行指定算子编译的过程文件存放路径，则需要通过DEBUG\_DIR参数指定，OP\_DEBUG\_LEVEL取值为0时，使用DEBUG\_DIR参数不生效。

**参数取值：**

- 0：（默认值）不开启算子debug功能，在当前执行路径**不生成**算子编译目录kernel\_meta。
- 1：开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）和TBE指令映射文件（算子cce文件\*.cce和python-cce映射文件\*\_loc.json），用于后续分析AICore Error问题。

    <!-- npu="950" id1 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件。
    <!-- end id1 -->

- 2：开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）和TBE指令映射文件（算子cce文件\*.cce和python-cce映射文件\*\_loc.json），用于后续分析AICore Error问题，同时设置为2，还会关闭编译优化开关、开启ccec调试功能（ccec编译器选项设置为-O0-g）。

    <!-- npu="950" id2 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件。
    <!-- end id2 -->

- 3：不开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹中**生成**\*.o（算子二进制文件）和\*.json文件（算子描述文件），分析算子问题时可参考。
- 4：不开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）、TBE指令映射文件（算子cce文件\*.cce）和UB融合计算描述文件（\{$kernel\_name\}\_compute.json），可在分析算子问题时进行问题复现、精度比对时使用。

    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件和UB融合计算描述文件。

> [!NOTE]说明
>
>- 若OP\_DEBUG\_LEVEL配置为0，同时配置了OP\_DEBUG\_CONFIG参数，该场景下在当前执行路径**会保留**算子编译目录kernel\_meta。
>- 若OP\_DEBUG\_LEVEL配置为0，同时设置了NPU\_COLLECT\_PATH环境变量，则会**始终保留**编译目录kernel\_meta；若设置了ASCEND\_WORK\_PATH环境变量，则保留在该环境变量指定路径下，若无ASCEND\_WORK\_PATH环境变量，则保留在当前执行路径。
>- 训练执行时，建议配置为0或3。如果需要进行问题定位，再选择调试开关选项1和2，是因为加入了调试功能会导致网络性能下降。
>- 配置为2（即开启ccec编译选项）时，会导致算子Kernel（\*.o文件）大小增大。动态Shape场景下，由于算子编译时会遍历可能的Shape场景，因此可能会导致算子Kernel文件过大而无法进行编译，此种场景下，建议不要配置ccec编译选项。
> 由于算子Kernel文件过大而无法编译的报错日志示例如下：
>
>   ```text
>   message:link error ld.lld: error: InputSection too large for range extension thunk ./kernel_meta_xxxxx.o
>    ```
>
>- debug功能开关打开场景下，若模型中含有如下通算融合算子，算子编译目录kernel\_meta中，不会生成下述算子的\*.o、\*.json、\*.cce文件。
>
>   MatMulAllReduce
>
>   MatMulAllReduceAddRmsNorm
>
>   AllGatherMatMul
>
>   MatMulReduceScatter
>
>   AlltoAllAllGatherBatchMatMul
>
>   BatchMatMulReduceScatterAlltoAll

**配置示例：**

```c++
{ge::ir_option::OP_DEBUG_LEVEL, "1"}
```

**产品支持情况：**

全量芯片支持。

## OP\_DEBUG\_CONFIG

Global Memory内存检测功能开关。此参数实际对应的options参数为`op_debug_config`。

**参数取值：**

取值为.cfg配置文件路径，配置文件内多个选项用英文逗号分隔：

- **oom**：算子**执行**过程中，检测Global Memory是否内存越界。
  - 配置该选项，算子编译时，在当前执行路径算子编译生成的kernel\_meta文件夹中保留.o（算子二进制文件）和.json文件（算子描述文件）。
  - 使用该选项后，在算子编译过程中会加入如下的检测逻辑，用户可以通过再使用**dump\_cce**参数，在生成的.cce文件中查看如下的代码。

    ```cce
    inline __aicore__ void  CheckInvalidAccessOfDDR(xxx) {
        if (access_offset < 0 || access_offset + access_extent > ddr_size) {
            if (read_or_write == 1) {
                trap(0X5A5A0001);
            } else {
                trap(0X5A5A0002);
            }
        }
    }
    ```

- **dump\_cce**：算子编译时，在当前执行路径算子编译生成的kernel\_meta文件夹中保留算子cce文件\*.cce，以及.o（算子二进制文件）和.json文件（算子描述文件）。
- **dump\_loc**：算子编译时，在当前执行路径算子编译生成的kernel\_meta文件夹中保留python-cce映射文件\*\_loc.json，以及.o（算子二进制文件）和.json文件（算子描述文件）。
- **ccec\_O0**：算子编译时，开启ccec编译器选项-O0，配置该选项**不会**对调试信息执行优化操作，用于后续分析AI Core Error问题。
- **ccec\_g**：算子编译时，开启ccec编译器选项-g，配置该选项**会**对调试信息执行优化操作，用于后续分析AI Core Error问题。
- **check\_flag**：算子**执行**时，检测算子内部流水线同步信号是否匹配。
  - 配置该选项，算子编译时，在当前执行路径算子编译生成的kernel\_meta文件夹中保留.o（算子二进制文件）和.json文件（算子描述文件）。
  - 使用该选项后，在算子编译过程中会加入如下的检测逻辑，用户可以通过再使用**dump\_cce**参数，在生成的.cce文件中查看如下的代码。

    ```cce
        set_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID0);
        set_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID1);
        set_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID2);
        set_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID3);
        ....
        pipe_barrier(PIPE_MTE3);
        pipe_barrier(PIPE_MTE2);
        pipe_barrier(PIPE_M);
        pipe_barrier(PIPE_V);
        pipe_barrier(PIPE_MTE1);
        pipe_barrier(PIPE_ALL);
        wait_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID0);
        wait_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID1);
        wait_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID2);
        wait_flag(PIPE_MTE3, PIPE_MTE2, EVENT_ID3);
        ...
    ```

    实际执行推理过程中，如果确实存在算子内部流水线同步信号不匹配，则最终会在**有问题的算子处超时报错，并终止程序**，报错信息示例为：

    ```text
    Aicore kernel execute failed, ..., fault kernel_name=算子名,...
    rtStreamSynchronizeWithTimeout execute failed....
    ```

**配置示例：**

```c++
{ge::ir_option::OP_DEBUG_CONFIG, "/root/test0.cfg"}
```

其中，test0.cfg文件信息为：

```text
op_debug_config=ccec_g,oom
```

**使用约束：**

算子编译时，如果用户不想编译所有AI Core算子，而是指定某些AI Core算子进行编译，则需要在上述test0.cfg配置文件中新增**OP\_DEBUG\_LIST**字段，算子编译时，只编译该列表指定的算子，并按照OP\_DEBUG\_CONFIG配置的选项进行编译。OP\_DEBUG\_LIST字段要求如下：

- 支持指定算子名称或者算子类型。
- 算子之间使用英文逗号分隔，若为算子类型，则以**OpType::TypeName**格式进行配置，支持算子类型和算子名称混合配置。
- 要编译的算子，必须放在OP\_DEBUG\_CONFIG参数指定的配置文件中。

配置示例如下，test0.cfg文件中增加如下信息：

```text
op_debug_config=ccec_g,oom
op_debug_list=GatherV2,opType::ReduceSum
```

模型编译时，_GatherV2,ReduceSum_算子按照ccec\_g,oom选项进行编译。

> [!NOTE]说明
>
>- 开启ccec编译选项的场景下（即ccec\_O0、ccec\_g选项），会增大算子Kernel（\*.o文件）的大小。动态shape场景下，由于算子编译时会遍历可能存在的所有场景，最终可能会导致由于算子Kernel文件过大而无法进行编译的情况，此种场景下，建议不要开启ccec编译选项。
> 由于算子kernel文件过大而无法编译的日志显示如下：
> message:link error ld.lld:  **error: InputSection too large for range extension thunk**  ./kernel\_meta\_xxxxx.o:\(xxxx\)
>- ccec编译器选项ccec\_O0和oom不能同时开启，可能会导致AICore Error报错，报错信息示例如下：
>
>   ```text
>   ...there is an aivec error exception, core id is 49, error code = 0x4 ...
>    ```
>
>- 若配置NPU\_COLLECT\_PATH环境变量，不支持打开“检测Global Memory是否内存越界”的开关（OP\_DEBUG\_CONFIG指定的配置文件中配置oom），否则编译出来的模型文件或算子kernel包在使用时会报错。
>- 配置编译选项oom、dump\_cce、dump\_loc时，若模型中含有如下通算融合算子，算子编译目录kernel\_meta中，不会生成下述算子的\*.o、\*.json、\*.cce文件。
>
>   MatMulAllReduce
>
>   MatMulAllReduceAddRmsNorm
>
>   AllGatherMatMul
>
>   MatMulReduceScatter
>
>   AlltoAllAllGatherBatchMatMul
>
>   BatchMatMulReduceScatterAlltoAll

**产品支持情况：**

全量芯片支持。

## OPTION\_SCREEN\_PRINT\_MODE

控制图编译过程是否打屏。此参数实际对应的options参数为`ge.screen_print_mode`。

**参数取值：**

- enable：打屏，默认为enable。
- disable：不打屏。

**配置示例：**

```c++
{ge::ir_option::OPTION_SCREEN_PRINT_MODE, "disable"}
```

**产品支持情况：**

全量芯片支持。

## OPTION\_EXPORT\_COMPILE\_STAT

配置图编译过程中是否生成算子融合信息（包括图融合和UB融合）的结果文件fusion\_result.json。此参数实际对应的options参数为`ge.exportCompileStat`。

该文件用于记录图编译过程中使用的融合规则，而精度比对中的FUSION\_SWITCH\_FILE参数可以关闭指定的融合规则，关闭的融合规则，不会在fusion\_result.json文件中呈现，文件中：

- session\_and\_graph\_id\__xx\_xx_：表示融合结果所属线程和图编号。
- graph\_fusion：表示图融合。
- ub\_fusion：表示UB融合

    <!-- npu="950" id3 -->
    **Ascend 950PR/Ascend 950DT不支持UB融合，不会生成该信息**。
    <!-- end id3 -->

- match\_times：表示图编译过程中匹配到的融合规则次数。
- effect\_times：表示实际生效的次数。
- repository\_hit\_times：优化UB融合知识库命中的次数

    <!-- npu="950" id4 -->
    **Ascend 950PR/Ascend 950DT不支持UB融合，不会生成该信息**。
    <!-- end id4 -->

**参数取值：**

- 0：不生成算子融合信息结果文件。
- 1：（默认值）程序运行正常退出时，生成算子融合信息结果文件。
- 2：图编译完成时，生成算子融合信息结果文件。即如果图编译已完成，后续程序提前中断，也会生成算子融合信息结果文件。

> [!NOTE]说明
>
>- 若未设置ASCEND\_WORK\_PATH环境变量，结果文件默认生成在执行脚本的当前路径；若设置了ASCEND\_WORK\_PATH环境变量，则保存路径为：$ASCEND\_WORK\_PATH/FE/$\{进程号\}/fusion\_result.json。环境变量详细说明请参见《环境变量参考》。
>- 通过FUSION\_SWITCH\_FILE参数关闭的融合规则，不会在fusion\_result.json文件中呈现。

**配置示例：**

```c++
{ge::ir_option::OPTION_EXPORT_COMPILE_STAT, "1"}
```

**产品支持情况：**

全量芯片支持。
