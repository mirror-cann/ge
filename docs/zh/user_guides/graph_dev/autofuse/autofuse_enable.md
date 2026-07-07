# AutoFuse开启方式

## 简介

### 前提条件

- 安装软件包：准备带有AI处理器的硬件环境，并安装驱动固件和CANN软件包，具体安装步骤请参见《CANN 软件安装》。
- GCC版本：要求9.5.0及以上，建议使用9.5.0版本。
- CMake：要求3.20.0版本及以上，建议使用3.20.0版本。
- 安装mspti，mspti有两种获取方式（开启PGO特性时需要安装该依赖）：
  - 直接获取编译好的匹配release包，获取链接<https://gitcode.com/Ascend/mspti/releases，安装命令如下：>

    ```bash
    ./mindstudio-profiler-tools-interface_{version}_{arch}.run --install --install-path={cann_install_path}
    ```

    --install-path：指定安装路径，需要为上述CANN软件包的安装路径。

  - 源码编译：
    - 将开源仓clone到本地，命令如下（以使用HTTPS协议为例）：

        ```bash
        git clone https://gitcode.com/Ascend/mspti.git
        ```

    - 执行编译，命令如下：

        ```bash
        bash script/build.sh
        ```

    - 替换CANN软件包文件：

        编译成功后，将csrc/include头文件目录复制到\{cann\_install\_path\}/cann/tools/mspti/include；将build/libmspti.so动态库目录复制到\{cann\_install\_path\}/cann/tools/mspti/lib64；cann\_install\_path需要为上述CANN软件包的安装路径。

- 安装完成后设置环境变量：

    安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行**source $_\{INSTALL\_DIR\}_/set\_env.sh**命令设置环境变量。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

### 启用AutoFuse

在TensorFlow图模式流程中，自动融合的开启方式由**环境变量控制**，包括功能控制与DFX控制，自动融合开箱只需要打开功能控制，DFX控制用于辅助定位或优化。**功能控制**的环境变量名为**AUTOFUSE\_FLAGS**，**DFX控制**的环境变量名为**AUTOFUSE\_DFX\_FLAGS**，环境变量值以字符串"--key=value"形式配置，每一项不同的key代表一个具体控制点， 多项配置使用英文分号分隔。

自动融合目前支持以下类别算子的融合，具体开启方式如下：

**表 1**  自动融合支持的算子类型

|融合类型|开启方式|Auto Tiling求解算法|Auto Tiling算法精度|融合算子生成时的中间过程文件|PGO调优算法|
|--|--|--|--|--|--|
|Elemwise|默认开启|默认轴排序算法|默认高精度求解|默认不保留|默认控核算法|
|Broadcast|默认开启|默认轴排序算法|默认高精度求解|默认不保留|默认控核算法|
|Reduce|环境变量显式控制|默认轴排序算法|默认高精度求解|默认不保留|默认控核算法|
|Concat|环境变量显式控制|默认轴排序算法|默认高精度求解|默认不保留|默认控核算法|

以Reduce和Concat算子为例，开启自动融合功能的配置示例如下：

- 功能控制：

    ```bash
    export AUTOFUSE_FLAGS="--enable_autofuse=true;--autofuse_enable_pass=reduce,concat"
    ```

- DFX控制：

    ```bash
    export AUTOFUSE_DFX_FLAGS="--att_accuracy_level=1;--att_profiling=true"
    ```

上述环境变量中每一项的详细解释请参见[AUTOFUSE\_FLAGS环境变量控制点](#autofuse_flags环境变量控制点)、[AUTOFUSE\_DFX\_FLAGS环境变量控制点](#autofuse_dfx_flags环境变量控制点)。

> [!NOTE]说明
>
>支持离线推理**静态shape场景**开启自动融合功能，方法为：
>
>1. 设置环境变量，开启自动融合功能，比如：
>
>       ```bash
>       export AUTOFUSE_FLAGS="--enable_autofuse=true"
>       ```
>
>       - 使用ATC离线模型编译工具转换模型，生成om离线模型（自动融合场景下，不支持单算子转om，即不支持--singleop参数）
>       - 使用[aclgrphBuildModel](../../../api/graph_engine_api/cpp/ge/aclgrphBuildModel.md)接口编译模型，生成om离线模型（自动融合场景下，不支持单算子转om，即不支持INSERT\_OP\_FILE参数）。
>
>2. 模型加载与推理
>
>    使用acl接口加载生成的om模型，完成推理。
>
>关于ATC工具详细使用方法请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。
>关于acl接口推理详细说明请参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”。

## AUTOFUSE\_FLAGS环境变量控制点

### --enable\_autofuse

控制整体自动融合功能是否开启。

**取值：**

- true：表示开启。
- false：（默认值）表示关闭。

不配置表示关闭整体自动融合功能，关闭时以下其他控制点全部失效，无论是否配置。

**配置示例：**

```text
--enable_autofuse=true
```

**使用约束：**

配置开启后，Elemwise算子与Broadcast算子间的自动融合能力便开启。

### --autofuse\_disable\_pass

控制拓展融合能力是否关闭。

**取值：**

- reduce：控制Reduce类算子融合能力关闭。
- concat：控制Concat类算子融合能力关闭。

配置多个取值时使用英文逗号分割，默认为空，Reduce类、Concat类算子的融合能力目前默认不开启。

**配置示例：**

```text
--autofuse_disable_pass=reduce,concat
```

**使用约束：**

与--autofuse\_enable\_pass不能同时配置相同的取值。

### --autofuse\_enable\_pass

控制拓展融合能力是否开启。

**取值：**

- reduce：控制Reduce融合能力开启。
- concat：控制Concat融合能力开启。

配置多个取值时使用英文逗号分割，默认为空，Reduce类、Concat类算子的融合能力目前默认不开启。

**配置示例：**

```text
--autofuse_enable_pass=reduce,concat
```

**使用约束：**

与--autofuse\_disable\_pass不能同时配置相同的取值。

### --autofuse\_enable\_pgo

控制是否开启PGO（Profile-Guided Optimization，基于采样数据的优化）调优。

PGO调优是通过预上板采样，选取表现相对更好的Tiling以提升模型执行性能。

**取值：**

- true：表示开启。
- false：（默认值）表示关闭。

**配置示例：**

```text
--autofuse_enable_pgo=true
```

**使用约束：**

仅支持对静态图调优，首次配置时，不能与其他Profiling功能同时开启，后续配置时无限制。

### --autofuse\_enhance\_precision\_blacklist

控制自动融合局部融合节点不做提升精度操作。

某个AscGraph中所有的AscIR都配置到黑名单中，该AscGraph才不会升精度。

**取值：**

- AscIR类型的字符串组合，多个类型使用英文逗号分割，默认值为空字符串，表示所有AscGraph都会提升精度。该参数中的AscIR类型是Dump图中对应节点type中的取值，例如Dump图中某节点type的取值为ge:Add，则实际配置为Add。
- all，表示将所有节点列入黑名单，从而禁止自动融合时的精度提升操作，避免不必要的升精度开销。

**配置示例：**

```text
--autofuse_enhance_precision_blacklist=Le,Where,Sub,Add,Sigmoid
```

**使用约束：**

- "Sum"、"Mean"、"Prod"不支持低精度类型，默认必须提升精度，因此不能配置到提升精度黑名单中，即使配置为all，仍旧提升精度。
- 不提升精度可以获得更高性能，但可能会引发精度问题。因此，在配置不提升精度后，用户需确保精度满足业务要求。
- Dump图的方法请参见《环境变量参考》  \> 图编译 \> DUMP\_GE\_GRAPH。

### --experimental\_enable\_jit\_executor\_v2

控制是否打开切图编译。

**取值：**

- true：表示开启。
- false：（默认值）表示关闭。

切图编译是将原始图在无法推导符号的边界进行断图处理，切成N个图，将上游图的输出作为下游图的输入hint继续符号推导并执行。

**配置示例：**

```text
--experimental_enable_jit_executor_v2=true
```

**使用约束：**

如下场景不支持切图：

- 动态分档场景
- 图上包含资源类算子（输入或者输入的类型是DT\_RESOURCE，如TensorArrayWrite）
- 图上包含V1版本控制算子（如："Switch","StreamSwitch","Merge","StreamMerge","Enter","Exit","LoopCond","NextIteration"）
- 开启数据预处理下沉
- 开启AOE调优的场景

### --max\_fusion\_size

配置最多可融合节点的个数。

**取值：**[0-uint64_t类型最大值]，配置为0表示不融合。

**配置示例：**

```text
--max_fusion_size=64
```

上述示例表示最多支持融合64个Ascir节点。

**使用约束：**
无

### --recomputation\_threshold

控制自动融合重计算阈值。

该参数用于设置单输出算子的引用阈值，当单输出节点引用个数超过阈值时，首次融合会在当前节点进行融合截断。

**取值：**0-255间的任意整数，默认为1。

**配置示例：**

```text
--recomputation_threshold=5
```

**使用约束：**

无。

## AUTOFUSE\_DFX\_FLAGS环境变量控制点

### --autofuse\_att\_algorithm

控制Auto Tiling求解算法选择。

**取值：**

- HighPerf：高性能算法。
- AxesReorder：（默认值）轴排序算法。

**配置示例：**

```text
--autofuse_att_algorithm=HighPerf
```

**使用约束：**

- 轴排序算法是默认算法，高性能算法属于试验性算法，不一定能获取到更好的算子执行性能，只是算法理论上限较高。
- 配置非法值时会恢复为默认值。

### --att\_accuracy\_level

控制Auto Tiling算法求解精度，理论上精度越高kernel性能越好。

**取值：**

- 1：（默认值）高精度求解。
- 0：低精度求解。

默认为低精度求解，保证Tiling耗时不至于过长。

**配置示例：**

```text
--att_accuracy_level=1
```

**使用约束：**

1. 高精度求解可能会得出更优的tiling解，但需要更长的Tiling执行时间，低精度求解则反之。
2. 配置非法值时会恢复为默认值。

### --att\_enable\_multicore\_ub\_tradeoff

用于控制att\_corenum\_threshold和att\_ub\_threshold功能是否开启。

**取值：**

- true：开启。
- false：（默认值）不开启。

**配置示例：**

```text
--att_enable_multicore_ub_tradeoff=true
```

**使用约束：**配置非法值时会恢复为默认值。

### --att\_ub\_threshold

控制Auto Tiling的Tiling策略，保证Tiling求解结果与算子实现结合，UB占用率不低于该控制点设置的取值（若UB占用率不满足指定阈值，则按可求解的最大UB占用率设置），该控制点可用于性能问题的定位。

**取值：**0-100间的任意整数，默认值为20。

**配置示例：**

```text
--att_ub_threshold=20
```

**使用约束：**

1. 该参数需要和[--att\_enable\_multicore\_ub\_tradeoff](#--att_enable_multicore_ub_tradeoff)配合使用，只有--att\_enable\_multicore\_ub\_tradeoff开启时，设置了--att\_ub\_threshold控制点才能生效。
2. 配置非法值时会恢复为默认值。

### --att\_corenum\_threshold

控制Auto Tiling的Tiling策略，保证Tiling求解结果与算子实现结合，多核利用率不低于该控制点设置的取值（若多核利用率不满足指定阈值，则按可求解的最大多核占用率设置），该控制点可用于性能问题的定位。

**取值：**0-100间的任意整数，默认值为40。

当[--att\_enable\_multicore\_ub\_tradeoff](#--att_enable_multicore_ub_tradeoff)开启时，默认值为40，否则不会设置该策略。

**配置示例：**

```text
--att_corenum_threshold=40
```

**使用约束：**

1. 该参数需要和--att\_enable\_multicore\_ub\_tradeoff配合使用，只有--att\_enable\_multicore\_ub\_tradeoff开启时，设置的--att\_corenum\_threshold控制点才能生效。
2. 配置非法值时会恢复为默认值。

### --att\_profiling

用于控制Auto Tiling的Profiling是否开启。

**取值：**

- true：开启Profiling特性。
- false：（默认值）关闭Profiling特性。

**配置示例：**

```text
--att_profiling=true
```

**使用约束：**

1. 该控制点仅用于定位Auto Tiling模块本身的执行时间问题。
2. 配置非法值时会恢复为默认值。

### --autofuse\_pgo\_algo

控制PGO调优算法，不同算法求Tiling方式不同。

**取值：**

- core\_select：（默认值）使用控核算法。
- pruning：使用剪枝算法。

默认为控核算法，保证Tiling耗时不至于过长，剪枝算法理论上可能求出更优的Tiling解，但需要远长于控核算法的Tiling时间。

**配置示例：**

```text
--autofuse_enable_pgo=true --autofuse_pgo_algo=core_select
```

**使用约束：**

1. 该参数需要与--autofuse\_enable\_pgo配合使用，只有--autofuse\_enable\_pgo开启时，设置的--autofuse\_pgo\_algo控制点才能生效。
2. 配置非法值时会恢复为默认值。

### --autofuse\_pgo\_step\_max

控制PGO剪枝算法步长。

**取值：**2-1024间任意2的幂次，默认为16。

**配置示例：**

```text
--autofuse_enable_pgo=true --autofuse_pgo_algo=pruning --autofuse_pgo_step_max=16
```

**使用约束：**

1. 该参数需要和--autofuse\_pgo\_algo配合使用，只有--autofuse\_pgo\_algo=pruning且生效时，设置的--autofuse\_pgo\_step\_max控制点才能生效。

    当step值较小时，可能会得出更优的tiling解，但需要更长的Tiling执行时间，step值较大时则反之。

2. 配置非法值时会恢复为默认值。

### --autofuse\_pgo\_topn

设置参与PGO静态调优的解的数量。

**取值：**

- 0：选择所有解参与静态调优。
- 任意正整数：表示可选解的数量，默认值为5。

**配置示例：**

```text
--autofuse_enable_pgo=true --autofuse_pgo_topn=5
```

**使用约束：**

1. 该参数需要与--autofuse\_enable\_pgo配合使用，只有--autofuse\_enable\_pgo开启时，设置的--autofuse\_pgo\_topn控制点才能生效。
2. 配置非法值时会恢复为默认值。

### --codegen\_compile\_debug

控制是否保留融合算子生成时的中间过程文件。

**取值：**

- true：保留文件。
- false：（默认为false）不保留文件。

**配置示例：**

```text
--codegen_compile_debug=true
```

设置为true，保留文件后，在脚本执行目录下将生成kernel\_meta\_\*文件夹，其中包含生成的kernel源码、Tiling源码、cmake工程以及编译结果，同时会在\{debug\_dir\}/autofuse\_compile\_debug/路径下生成融合算子Schedule各个过程中的AscGraph Dump图。

**使用约束：**无

### --debug\_dir

指定融合中AscGraph dump图保存路径，支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符，执行用户需对该路径具有读、写、执行权限。若未指定该选项，文件将保存至脚本执行的当前目录。

**配置示例：**

```text
--codegen_compile_debug=true;--debug_dir=/path/to/dump
```

**使用约束：**需要先配置--codegen\_compile\_debug=true;打开自动融合debug开关。

### --disable\_lifting

用于控制lifting功能是否关闭。

自动融合会将算子融合成名为AscBackend的新算子，lifting则将未能满足条件的AscBackend回滚到原算子。

**取值：**

- true：关闭lifting功能。
- false：（默认值）开启lifting功能。

**配置示例：**

```text
--disable_lifting=true
```

**使用约束：**

该控制点仅用于定位Ascbackend回滚结构的分析问题，开启该控制点可能会导致ApplyAdamD算子精度异常。

### --skip\_node\_names\_cfg

设置跳过融合的算子名字或算子类型。

**取值：** 设置了算子名称或者算子类型的配置文件（.ini格式）路径以及文件名，每个算子单独一行，支持同时设置算子名称或者算子类型，算子类型必须为基于Ascend IR定义的算子的类型。

**配置示例：**

ini配置文件示例如下：

```ini
[ByNodeName]
op_name1
op_name2
[ByNodeType]
op_type1
op_type2
```

参数使用示例：

```text
--skip_node_names_cfg=./skil_node.ini
```

**使用约束：**

配置文件中的内容为非法值时，该配置项不生效。
