# --op\_debug\_config

## 产品支持情况

全量芯片支持

## 功能说明

算子debug功能配置参数，用于控制Global Memory内存检测和算子编译调试选项。

## 关联参数

无。

## 参数取值

**参数值：**
配置文件路径及文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**

配置文件中支持配置如下选项，多个选项使用英文逗号分隔。

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

        ```
        Aicore kernel execute failed, ..., fault kernel_name=算子名,...
        rtStreamSynchronizeWithTimeout execute failed....
        ```

> [!NOTE]说明
>
>- 配置ccec编译选项（即ccec\_O0、ccec\_g选项）时，会导致算子Kernel（\*.o文件）大小增大。动态Shape场景下，由于算子编译时会遍历可能的Shape场景，因此可能会导致算子Kernel文件过大而无法进行编译，此种场景下，建议不要配置ccec编译选项。
>
>   由于算子Kernel文件过大而无法编译的报错日志示例如下：
>
>   ```console
>   message:link error ld.lld: error: InputSection too large for range extension thunk ./kernel_meta_xxxxx.o:
>    ```
>
>- ccec编译器选项ccec\_O0和oom不能同时开启，可能会导致AICore Error报错，报错信息示例如下：
>
>   ```console
>   ...there is an aivec error exception, core id is 49, error code = 0x4 ...
>    ```
>
>- 若配置NPU\_COLLECT\_PATH环境变量，不支持打开“检测Global Memory是否内存越界”的开关（--op\_debug\_config指定的配置文件配置为oom），否则编译出来的模型文件或算子kernel包在使用时会报错。
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

## 推荐配置及收益

无。

## 示例

假设配置文件名称为gm\_debug.cfg，文件内容配置示例如下：

```text
op_debug_config=ccec_g,oom
```

将该文件上传到ATC工具所在服务器，例如上传到_$HOME/module_，使用示例如下：

```bash
atc --op_debug_config=$HOME/module/gm_debug.cfg ...
```

## 使用约束

算子编译时，如果用户不想编译所有AI Core算子，而是指定某些AI Core算子进行编译，则需要在上述_gm\_debug.cfg_配置文件中新增**op\_debug\_list**字段，算子编译时，只编译该列表指定的算子，并按照op\_debug\_config配置的选项进行编译。**op\_debug\_list**字段要求如下：

- 支持指定算子名称或者算子类型。
- 算子之间使用英文逗号分隔，若为算子类型，则以**OpType::TypeName**格式进行配置，支持算子类型和算子名称混合配置。
- 要编译的算子，必须放在--op\_debug\_config参数指定的配置文件中。算子类型必须为基于Ascend IR定义的算子的类型，算子类型查看方法请参见[如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系](../FAQ/operator_correspondence_guide.md)。

配置示例如下：

在--op\_debug\_config参数指定的配置文件（例如gm\_debug.cfg）中增加如下信息：

```text
op_debug_config=ccec_g,oom
op_debug_list=GatherV2,OpType::ReduceSum
```

将该文件上传到ATC工具所在服务器，例如上传到_$HOME/module_，使用示例如下：

```bash
atc --op_debug_config=$HOME/module/gm_debug.cfg ...
```

实际模型转换时，*GatherV2,ReduceSum*算子按照ccec\_g,oom选项进行编译。
