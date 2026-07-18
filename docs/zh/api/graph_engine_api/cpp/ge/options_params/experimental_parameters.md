# 试验参数

## ge.autoMultistreamParallelMode

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

该参数适用于静态/动态shape图场景，开发者可通过配置此参数控制多流并行模式的自动分配策略，以提升图执行性能。

**参数取值：**

- cv：代表开启Cube算子与Vector算子的并行执行功能。
- LoadBalance:N，负载均衡算法，将所有算子均匀分布在N条流上执行。N为最大流数量，正整数，取值范围\[1,64\]。若N取值超过了实际可用核数，性能可能会降低。
- MainStream:N，主流算法，串行算子分布在主流上执行，其他可并行算子分布在其他流上执行。N为最大流数量，正整数，取值范围\[1,64\]。若N取值超过了实际可用核数，性能可能会降低。
- None（默认值），不启用任何多流并行优化。

> [!NOTE]说明
>
>- 该参数仅限于推荐类型网络使用。
>- 动态shape多流下使用此功能需要先通过ENABLE\_DYNAMIC\_SHAPE\_MULTI\_STREAM使能动态shape多流，再配置此参数。
> 关于环境变量详细说明请参见《环境变量参考》。

**配置示例：**

```c++
{"ge.autoMultistreamParallelMode", "cv"};
```

**必选/可选**：可选

**生效级别**：session/graph

## ge.build\_inner\_model

**当前版本暂不支持。**

## ge.deterministicLevel

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

确定性计算级别。

默认情况下，确定性级别为0，即不开启确定性计算，是否开启确定性计算的选项ge.deterministic也需要是0。当需要开启确定性计算时，确定性级别需要设置为1，同时确定性计算选项ge.deterministic也需设置为1。当开启强一致性计算时，级别需要设置为2，同时确定性计算选项ge.deterministic也需设置为1。

当开启强一致性计算功能时（ge.deterministicLevel=2），计算结果是确定的，多次执行将产生相同的输出。此外，计算结果与数据的位置无关。例如，在进行矩阵乘时，不同行的累加顺序可能不同，这可能会导致相同数据在不同行的计算结果出现细微差异。但在启用强一致性计算的情况下，只要输入相同，即使在不同的行中，计算结果都将保持一致。

默认情况下，强一致性计算功能不会启用。在这种默认模式下，相同数据出现在不同行时，可能会产生计算结果上的不一致。

出于性能考虑，通常建议不开启强一致性计算，因为它会降低算子的计算速度，影响整体效率。只有在以下情形下，才建议启用该功能：需要严格保证相同数据在不同位置上的结果一致性，或者正在对模型进行精度调整和调试，以优化整体表现。

参数取值：

- 0：（默认值）不开启确定性计算。
- 1：开启确定性计算。
- 2：开启强一致性计算。

**配置示例：**

```c++
{"ge.deterministic", "0"};
{"ge.deterministicLevel", "0"};
```

**使用约束：**

该配置项需要和ge.deterministic配合使用。

**必选/可选**：可选

**生效级别**：全局

## ge.disableOptimizations

**该参数为调试参数，当前不支持应用于商用产品中，后续版本会作为正式特性更新发布。**

<!-- npu="A3,910b" id1 -->
该参数仅适用于如下产品：

<!-- npu="910b" id2 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id2 -->

<!-- npu="A3" id3 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id3 -->
<!-- end id1 -->

用于指定**关闭**的某一个或者多个编译优化pass。

当前仅支持关闭如下pass：

```c++
"RemoveSameConstPass","ConstantFoldingPass","TransOpWithoutReshapeFusionPass"
```

注意：

1. 多个pass之间使用英文逗号分隔。
2. 若用户指定关闭了其他pass，graph编译时不会处理，只会打印warning级别的日志。
3. 若指定关闭了ConstantFoldingPass，graph编译或运行时可能会失败。
4. 如果同时配置了其他编译优化选项，比如ge.oo.constantFolding，则ge.disableOptimizations优先级更高。

**配置示例**：

- 关闭单个pass

    ```c++
    std::map <AscendString, AscendString> session_options = {
    {"ge.disableOptimizations", "RemoveSameConstPass"}
    };
    ```

- 关闭多个pass

    ```c++
    std::map <AscendString, AscendString> session_options = {
    {"ge.disableOptimizations", "RemoveSameConstPass, ConstantFoldingPass"}
    };
    ```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.jit\_compile

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

模型编译时，选择是优先在线编译算子，还是优先使用已编译好的算子二进制文件。

**参数取值：**

- 0：优先查找系统中已编译好的算子二进制文件，如果能查找到，则不再编译算子，编译性能更优；如果查找不到，则再编译算子。
- 1（默认值）：在线编译算子，系统根据得到的图信息进行融合及优化，从而编译出运行性能更优的算子。
- 2：针对静态shape网络，在线编译算子；针对动态shape网络，优先查找系统中已编译好的算子二进制，如果查找不到对应的二进制，再编译算子。

**配置示例：**

```c++
{"ge.jit_compile", "1"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.oo.constantFolding

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

是否开启常量折叠优化。

常量折叠是将计算图中可以预先确定输出值的节点替换成常量，并对计算图进行一些结构简化的操作。

**参数取值：**

- true：（默认值）开启。
- false：关闭。

**配置示例：**

```c++
{"ge.oo.constantFolding", "true"};
```

**使用约束：**

如果同时配置了其他编译优化选项，比如ge.disableOptimizations，则ge.disableOptimizations优先级更高。

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.oo.deadCodeElimination

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

是否开启死边消除优化。

死边消除：switch死边消除，switch的pred输入（1号输入）为const节点时，根据const的值消除其中一条分支：const为true时，消除false分支；const为false时，消除true分支。

**参数取值：**

- true：（默认值）开启。
- false：关闭。

**配置示例：**

```c++
{"ge.oo.deadCodeElimination", "true"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.oo.level

**调试功能扩展参数，当前不支持应用于商用产品中，后续版本会作为正式功能更新发布。**

图编译多级优化选项，包括子图优化、整图优化、静态Shape模型下沉等。

静态Shape模型下沉：静态Shape模型在编译时即可确定所有算子的输入输出Shape，完成模型级内存编排、算子的Tiling计算等Host侧计算，在模型加载时整体下发到Device流上，但不立即执行，通过下发模型执行Task触发模型中所有Task的执行。

**参数取值：**

- O1：会关闭所有图融合和UB融合Pass，只做促成静态下沉的相关优化，如InferShape（进行输出Tensor的shape推导）、常量折叠、死边消除等。

    <!-- npu="950" id4 -->
    Ascend 950PR/Ascend 950DT不支持UB融合，只关闭图融合Pass。
    <!-- end id4 -->

- O3：（默认值）开启所有优化。

**使用约束：**

取值为O1时，会关闭所有图融合和UB融合PASS，只开启静态下沉的相关PASS，但是如下路径文件中的图融合PASS，由于关闭后会有功能问题，会默认开启：

`${INSTALL_DIR}/x86_64-linux/lib64/plugin/opskernel/fusion_pass/config/fusion_config.json`文件中"ExceptionalPassOfO1Level"字段下的所有图融合PASS。

其中$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**配置示例：**

```c++
{"ge.oo.level", "O3"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph
