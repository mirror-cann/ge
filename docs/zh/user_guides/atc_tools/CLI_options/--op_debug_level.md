# --op\_debug\_level

## 产品支持情况

全量芯片支持

## 功能说明

算子debug功能开关。

## 关联参数

如果要自行指定算子编译的过程文件存放路径，需--op\_debug\_level（取值为非0）与[--debug\_dir](--debug_dir.md)配合使用。

## 参数取值

**参数值：**

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

    <!-- npu="950" id3 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件和UB融合计算描述文件。
    <!-- end id3 -->

**参数值约束：**

- 进行模型转换时，建议配置为0、3或4。如果需要定位AICore Error问题，则需要将参数值设置为1或2。设置为1或2后，由于加入了调试功能，会导致网络性能下降。
- 若--op\_debug\_level配置为0，同时配置了[--op\_debug\_config](--op_debug_config.md)参数，该场景下在执行atc命令当前路径**会保留**算子编译目录kernel\_meta。
- 若--op\_debug\_level配置为0，同时设置了NPU\_COLLECT\_PATH环境变量，则会**始终保留**编译目录kernel\_meta；若设置了ASCEND\_WORK\_PATH环境变量，则保留在该环境变量指定路径下，若无ASCEND\_WORK\_PATH环境变量，则保留在当前执行路径。
- 配置为2（即开启ccec编译选项）时，会导致算子Kernel（\*.o文件）大小增大。动态Shape场景下，由于算子编译时会遍历可能的Shape场景，因此可能会导致算子Kernel文件过大而无法进行编译，此种场景下，建议不要配置ccec编译选项。

    由于算子Kernel文件过大而无法编译的报错日志示例如下：

    ```console
    message:link error ld.lld: error: InputSection too large for range extension thunk ./kernel_meta_xxxxx.o
    ```

- debug功能开关打开场景下，若模型中含有如下通算融合算子，算子编译目录kernel\_meta中，不会生成下述算子的\*.o、\*.json、\*.cce文件。

    MatMulAllReduce

    MatMulAllReduceAddRmsNorm

    AllGatherMatMul

    MatMulReduceScatter

    AlltoAllAllGatherBatchMatMul

    BatchMatMulReduceScatterAlltoAll

## 推荐配置及收益

无。

## 示例

```bash
atc --op_debug_level=1 ...
```

## 使用约束

- 算子编译生成的调试文件存储路径，除[--debug\_dir](--debug_dir.md)参数设置的方式外，还可以配置环境变量ASCEND\_WORK\_PATH，几种方式优先级为：配置参数“[--debug\_dir](--debug_dir.md)”\>环境变量ASCEND\_WORK\_PATH \>默认存储路径。

    关于ASCEND\_WORK\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

- 该参数优先级高于算子编译接口（TBE DSL的build接口或者TBE TIK的BuildCCE接口）中的**tbe\_debug\_level**的值。
