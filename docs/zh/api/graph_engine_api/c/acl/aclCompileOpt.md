# aclCompileOpt

## 定义

```c
typedef enum {
    ACL_PRECISION_MODE,           // 网络模型的算子精度模式
    ACL_AICORE_NUM,               // 模型编译时使用的AI Core数量
    ACL_AUTO_TUNE_MODE,           // 算子的自动调优模式
    ACL_OP_SELECT_IMPL_MODE,      // 选择算子是高精度实现还是高性能实现
    ACL_OPTYPELIST_FOR_IMPLMODE,  // 列举算子类型的列表，该列表中的算子使用ACL_OP_SELECT_IMPL_MODE指定的模式
    ACL_OP_DEBUG_LEVEL,           // TBE算子编译debug功能开关
    ACL_DEBUG_DIR,                // 保存模型转换、网络迁移过程中算子编译生成的调试相关过程文件的路径，包括算子.o/.json/.cce等文件。
    ACL_OP_COMPILER_CACHE_MODE,   // 算子编译磁盘缓存模式
    ACL_OP_COMPILER_CACHE_DIR,    // 算子编译磁盘缓存的目录
    ACL_OP_PERFORMANCE_MODE,      // 通过该选项设置是否按照算子执行高性能的方式编译算子
    ACL_OP_JIT_COMPILE,           // 选择是在线编译算子，还是使用已编译的算子二进制文件
    ACL_OP_DETERMINISTIC,         // 是否开启确定性计算
    ACL_CUSTOMIZE_DTYPES,         // 模型编译时自定义某个或某些算子的计算精度
    ACL_OP_PRECISION_MODE,        // 指定算子内部处理时的精度模式，支持指定一个算子或多个算子。
    ACL_ALLOW_HF32,               // hf32是AI处理器推出的专门用于算子内部计算的精度类型，当前版本不支持
    ACL_PRECISION_MODE_V2,        // 网络模型的算子精度模式，相比ACL_PRECISION_MODE选项，ACL_PRECISION_MODE_V2是新版本中新增的，精度模式更多，同时原有精度模式选项值语义更清晰，便于理解
    ACL_OP_DEBUG_OPTION           // 当前仅支持配置为oom，表示开启Global Memory访问越界检测
} aclCompileOpt;
```

## ACL\_PRECISION\_MODE取值说明

用于配置网络模型的算子精度模式。如果不配置该编译选项，默认采用allow\_fp32\_to\_fp16。

- **force\_fp32/cube\_fp16in\_fp32out：**

    配置为force\_fp32或cube\_fp16in\_fp32out，效果等同，该选项用来表示AI Core中该算子既支持float32又支持float16数据类型时，系统内部都会根据算子类型不同，选择不同的处理方式。cube\_fp16in\_fp32out为新版本中新增的，对于矩阵计算类算子，该选项语义更清晰。

    - 对于矩阵计算类算子，系统内部会按算子实现的支持情况处理：
        1. 优先选择输入数据类型为float16且输出数据类型为float32；
        2. 如果1中的场景不支持，则选择输入数据类型为float32且输出数据类型为float32；
        3. 如果2中的场景不支持，则选择输入数据类型为float16且输出数据类型为float16；
        4. 如果3中的场景不支持，则报错。

    - 对于矢量计算类算子，表示原图中算子精度为float16或bfloat16，强制选择float32。

        如果原图中存在部分算子，在AI Core中该算子的实现不支持float32，比如某算子仅支持float16类型，则该参数不生效，仍然使用支持的float16；如果在AI Core中该算子的实现不支持float32，且又配置了黑名单（precision\_reduce = false），则会使用float32的AI CPU算子；如果AI CPU算子也不支持，则执行报错。

- **force\_fp16（默认值）：**

    表示原图中算子精度为float16、bfloat16和float32时，强制选择float16。

- **allow\_fp32\_to\_fp16：**
    - 对于矩阵类算子：
        - 如果原图中算子精度为float32，优先降低精度到float16，如果AI Core中算子不支持float16，则继续选择float32，如果AI Core中算子不支持float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。
        - 如果原图中算子精度为bfloat16，则优先使用原图精度bfloat16，如果AI Core中算子不支持bfloat16，则选择float32，如果AI Core中算子不支持float32，则直接降低精度到float16；如果AI Core中算子不支持float16，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

    - 对于矢量类算子，优先保持原图精度：
        - 如果原图中算子精度为float32，则优先使用原图精度float32，如果AI Core中算子不支持float32，则直接降低精度到float16；如果AI Core中算子不支持float16，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。
        - 如果原图中算子精度为bfloat16，则优先使用原图精度bfloat16，如果AI Core中算子不支持bfloat16，则选择float32，如果AI Core中算子不支持float32，则直接降低精度到float16；如果AI Core中算子不支持float16，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

- **must\_keep\_origin\_dtype：**

    保持原图精度。

    - 如果原图中某算子精度为float16，AI Core中该算子的实现不支持float16、仅支持float32和bfloat16，则系统内部会自动采用高精度float32。
    - 如果原图中某算子精度为float16，AI Core中该算子的实现不支持float16、仅支持bfloat16，则会使用float16的AI CPU算子；如果AI CPU算子也不支持，则执行报错。
    - 如果原图中某算子精度为float32，AI Core中该算子的实现不支持float32类型、仅支持float16类型，则会使用float32的AI CPU算子；如果AI CPU算子也不支持，则执行报错。

- **allow\_mix\_precision/allow\_mix\_precision\_fp16：**

    配置为allow\_mix\_precision或allow\_mix\_precision\_fp16，效果等同，均表示使用混合精度float16、bfloat16和float32数据类型来处理神经网络的过程。allow\_mix\_precision\_fp16为新版本中新增的，语义更清晰，便于理解。

    针对原始模型中float32和bfloat16数据类型的算子，按照内置的优化策略，自动将部分float32和bfloat16的算子降低精度到float16，从而在精度损失很小的情况下提升系统性能并减少内存使用。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

    - 若取值为true（白名单），则表示允许将当前float32和bfloat16类型的算子，降低精度到float16。
    - 若取值为false（黑名单），则不允许将当前float32和bfloat16类型的算子降低精度到float16，相应算子仍旧使用float32或bfloat16精度。
    - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

    <!-- npu="950,A3,910b,310b" id1 -->
- **allow\_mix\_precision\_bf16：**

    表示使用混合精度bfloat16和float32数据类型来处理神经网络的过程。针对原始模型中float32数据类型的算子，按照内置的优化策略，自动将部分float32的算子降低精度到bfloat16，从而在精度损失很小的情况下提升系统性能并减少内存使用；如果AI Core中算子不支持bfloat16和float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

    - 若取值为true（白名单），则表示允许将当前float32类型的算子，降低精度到bfloat16。
    - 若取值为false（黑名单），则不允许将当前float32类型的算子降低精度到bfloat16，相应算子仍旧使用float32精度。
    - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。
    <!-- end id1 -->

    <!-- npu="950,A3,910b,310b" id2 -->
- **allow\_fp32\_to\_bf16：**
    - 如果原图中算子精度为float32，则优先使用原图精度float32，如果AI Core中算子不支持float32，则降低精度到bfloat16；如果AI Core中算子不支持bfloat16，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。
    - 如果原图中算子精度为bfloat16，则优先使用原图精度bfloat16，如果AI Core中算子不支持bfloat16，则选择float32，如果AI Core中算子不支持float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。
    <!-- end id2 -->

## ACL\_AICORE\_NUM取值说明

用于配置模型编译时使用的AI Core数量。

<!-- npu="950,A3,910b,910,310p,310b" id3 -->
当前版本设置无效。
<!-- end id3 -->

<!-- npu="IPV350" id4 -->
当前版本设置无效。
<!-- end id4 -->

## ACL\_AUTO\_TUNE\_MODE取值说明

用于配置算子的自动调优模式。**该参数后续废弃，请勿配置，否则后续版本可能存在兼容性问题。**

## ACL\_OP\_SELECT\_IMPL\_MODE取值说明

用于选择算子是高精度实现还是高性能实现。如果不配置该编译选项，默认采用high\_precision。

- **high\_precision**：表示算子采用高精度实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为$\{INSTALL\_DIR\}/opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/high\_precision.ini。

    为保持兼容，该参数仅对high\_precision.ini文件中算子列表生效，通过该列表可以控制算子生效的范围并保证之前版本的网络模型不受影响。

- **high\_performance**：（默认值）表示算子采用高性能实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为$\{INSTALL\_DIR\}/opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/high\_performance.ini。

    为保持兼容，该参数仅对high\_performance.ini文件中算子列表生效，通过该列表可以控制算子生效的范围并保证之前版本的网络模型不受影响。

- **high\_precision\_for\_all**：表示算子采用高精度实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为$\{INSTALL\_DIR\}/opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/high\_precision\_for\_all.ini，该文件中列表后续可能会跟随版本更新。

    **该实现模式不保证兼容**，如果后续新的软件包中有算子新增了实现模式（即配置文件中新增了某个算子的实现模式），之前版本使用high\_precision\_for\_all的网络模型，在新版本上性能可能会下降。

- **high\_performance\_for\_all**：表示算子采用高性能实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为$\{INSTALL\_DIR\}/opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/high\_performance\_for\_all.ini，该文件中列表后续可能会跟随版本更新。

    **该实现模式不保证兼容**，如果后续新的软件包中有算子新增了实现模式（即配置文件中新增了某个算子的实现模式），之前版本使用high\_performance\_for\_all的网络模型，在新版本上精度可能会下降。

## ACL\_OPTYPELIST\_FOR\_IMPLMODE取值说明

通过ACL\_OPTYPELIST\_FOR\_IMPLMODE选项设置算子类型的列表（多个算子使用英文逗号进行分隔），与ACL\_OP\_SELECT\_IMPL\_MODE选项配合使用，设置列表中的算子通过高精度实现或高性能实现。

## ACL\_OP\_DEBUG\_LEVEL取值说明

用于配置TBE算子编译debug功能开关。

**取值说明如下：**

- 0：（默认值）不开启算子debug功能，在当前执行路径**不生成**算子编译目录kernel\_meta。
- 1：开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）和TBE指令映射文件（算子cce文件\*.cce和python-cce映射文件\*\_loc.json），用于后续分析AICore Error问题。

    <!-- npu="950" id5 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件。
    <!-- end id5 -->

- 2：开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）和TBE指令映射文件（算子cce文件\*.cce和python-cce映射文件\*\_loc.json），用于后续分析AICore Error问题，同时设置为2，还会关闭编译优化开关、开启ccec调试功能（ccec编译器选项设置为-O0-g）。

    <!-- npu="950" id6 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件。
    <!-- end id6 -->

- 3：不开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹中**生成**\*.o（算子二进制文件）和\*.json文件（算子描述文件），分析算子问题时可参考。
- 4：不开启算子debug功能，在当前执行路径生成kernel\_meta文件夹，并在该文件夹下**生成**\*.o（算子二进制文件）、\*.json文件（算子描述文件）、TBE指令映射文件（算子cce文件\*.cce）和UB融合计算描述文件（\{$kernel\_name\}\_compute.json），可在分析算子问题时进行问题复现、精度比对时使用。

    <!-- npu="950" id7 -->
    Ascend 950PR/Ascend 950DT不会生成TBE指令映射文件和UB融合计算描述文件。
    <!-- end id7 -->

**配置约束如下：**

- 配置为2（即开启ccec编译选项）时，会导致算子Kernel（\*.o文件）大小增大。动态Shape场景下，由于算子编译时会遍历可能的Shape场景，因此可能会导致算子Kernel文件过大而无法进行编译，此种场景下，建议不要配置ccec编译选项。

    由于算子Kernel文件过大而无法编译的报错日志示例如下：

    ```bash
    message:link error ld.lld: error: InputSection too large for range extension thunk ./kernel_meta_xxxxx.o
    ```

- debug功能开关打开场景下，若模型中含有如下通算融合算子，算子编译目录kernel\_meta中，不会生成下述算子的\*.o、\*.json、\*.cce文件。

    MatMulAllReduce

    MatMulAllReduceAddRmsNorm

    AllGatherMatMul

    MatMulReduceScatter

    AlltoAllAllGatherBatchMatMul

    BatchMatMulReduceScatterAlltoAll

## ACL\_DEBUG\_DIR取值说明

用于配置保存模型转换、网络迁移过程中算子编译生成的调试相关过程文件的路径（默认路径：执行应用的当前路径/kernel\_meta），包括算子.o/.json/.cce等文件。具体生成哪些文件以ACL\_OP\_DEBUG\_LEVEL选项设置的取值为准。

路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

关于配置算子编译文件的缓存目录，除此处设置枚举值的方式，还可以配置环境变量ASCEND\_WORK\_PATH，几种方式的优先级为：本节设置枚举值的方式 \> 设置环境变量 \> 默认路径。环境变量的详细配置说明请参见《环境变量参考》。

## ACL\_OP\_COMPILER\_CACHE\_MODE取值说明

用于配置算子编译磁盘缓存模式。该编译选项需要与ACL\_OP\_COMPILER\_CACHE\_DIR配合使用。

- enable：表示启用算子编译缓存。启用后可以避免针对相同编译参数及算子参数的算子重复编译，从而提升编译速度。
- force：启用算子编译缓存功能，区别于enable模式，force模式下会强制刷新缓存，即先删除已有缓存，再重新编译并加入缓存。比如当用户的python变更、依赖库变更、算子调优后知识库变更等，需要先指定为force用于先清理已有的缓存，后续再修改为enable模式，以避免每次编译时都强制刷新缓存。
- disable：（默认值）表示禁用算子编译缓存，算子重新编译。

若同时打开调试开关（将ACL\_OP\_DEBUG\_LEVEL选项配置为非0值），则系统会忽略ACL\_OP\_COMPILER\_CACHE\_MODE选项的配置，对调试场景的编译结果不做缓存。

启用算子编译缓存功能时，可以通过**配置文件**（编译算子时在ACL\_OP\_COMPILER\_CACHE\_DIR选项指定的路径下自动生成op\_cache.ini配置文件）、**环境变量**两种方式来设置缓存文件夹的磁盘空间大小：

1. 通过配置文件op\_cache.ini设置

    若op\_cache.ini文件不存在，则需要手动创建。打开该文件，增加如下信息：

    ```json
    #配置文件格式，必须包含，自动生成的文件中默认包括如下信息，手动创建时，需要输入
    [op_compiler_cache]
    #限制某个芯片下缓存文件夹的磁盘空间的大小，默认值为500，取值需为整数，单位为MB
    max_op_cache_size=500
    #设置需要保留缓存的空间大小比例，取值范围：[1,100]，默认值为50，单位为百分比；例如取值为80表示缓存空间不足时，删除缓存文件，保留80%缓存空间
    remain_cache_size_ratio=50
    ```

    - 上述文件中的max\_op\_cache\_size和remain\_cache\_size\_ratio参数取值都有效时，op\_cache.ini文件才会生效。
    - 当编译缓存文件大小超过“max\_op\_cache\_size”的设置值，且超过半小时缓存文件未被访问时，缓存文件就会老化（算子编译时，不会因为编译缓存文件大小超过设置值而中断，所以当“max\_op\_cache\_size”设置过小时，会出现实际编译缓存文件大小超过此设置值的情况）。
    - 若需要关闭编译缓存老化功能，可将“max\_op\_cache\_size”设置为“-1”，此时访问算子缓存时不会更新访问时间，算子编译缓存不会老化，磁盘空间使用默认大小500MB。
    - 若多个使用者使用相同的缓存路径，建议使用配置文件的方式进行设置，该场景下op\_cache.ini文件会影响所有使用者。

2. 通过环境变量设置

    该场景下，开发者可以通过环境变量ASCEND\_MAX\_OP\_CACHE\_SIZE来限制某个芯片下缓存文件夹的磁盘空间的大小，当编译缓存空间大小达到ASCEND\_MAX\_OP\_CACHE\_SIZE设置的取值，且超过半个小时缓存文件未被访问时，缓存文件就会老化。可通过环境变量ASCEND\_REMAIN\_CACHE\_SIZE\_RATIO设置需要保留缓存的空间大小比例。

    配置示例如下：

    ```bash
    # ASCEND_MAX_OP_CACHE_SIZE环境变量默认值为500，取值需为整数，单位为MB
    export ASCEND_MAX_OP_CACHE_SIZE=500
    # ASCEND_REMAIN_CACHE_SIZE_RATIO环境变量取值范围：[1,100]，默认值为50，单位为百分比；例如取值为80表示缓存空间不足时，删除缓存文件，保留80%缓存空间
    export ASCEND_REMAIN_CACHE_SIZE_RATIO=50
    ```

    - 通过环境变量配置，只对当前用户生效。
    - 若需要关闭编译缓存老化功能，可将环境变量“**ASCEND\_MAX\_OP\_CACHE\_SIZE**”设置为“-1”，此时访问算子缓存时不会更新访问时间，算子编译缓存不会老化，磁盘空间使用默认大小500MB。

**若同时配置了op\_cache.ini文件和环境变量，则优先读取op\_cache.ini文件中的配置项，若op\_cache.ini文件和环境变量都未设置，则读取系统默认值：默认磁盘空间大小500MB，默认保留缓存空间的50%。**

## ACL\_OP\_COMPILER\_CACHE\_DIR取值说明

用于配置算子编译文件的缓存目录（默认路径：$HOME/atc\_data）。该编译选项需要与ACL\_OP\_COMPILER\_CACHE\_MODE配合使用。

路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

如果设置了ACL\_OP\_DEBUG\_LEVEL编译选项，则只有编译选项值为0才会启用编译缓存功能，其它取值禁用编译缓存功能。

关于配置算子编译文件的缓存目录，除此处设置枚举值的方式，还可以配置环境变量ASCEND\_CACHE\_PATH，几种方式的优先级为：本节设置枚举值的方式 \> 设置环境变量 \> 默认路径。环境变量的详细配置说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

## ACL\_OP\_PERFORMANCE\_MODE取值说明

通过该选项设置是否按照算子执行高性能的方式编译算子。**该参数已废弃，请勿配置，否则后续版本可能存在兼容性问题。**

取值范围：

- normal：算子编译时按照编译性能最高的方式进行编译。默认采用该方式。
- high：算子编译时应该按照算子执行性能最好的方式去泛化编译。

## ACL\_OP\_JIT\_COMPILE取值说明

选择是在线编译算子，还是使用已编译的算子二进制文件。

取值范围：

- enable：在线编译算子，系统根据得到的算子信息进行优化，从而编译出运行性能更优的算子。固定Shape网络场景下，建议设置为enable。
- disable：优先查找系统中的已编译好的算子二进制文件，如果能查找到，则不再编译算子，编译性能更优；如果查找不到，则再编译算子。动态Shape网络场景下，建议设置为disable。

各产品型号的默认值不同：

<!-- npu="950" id8 -->
Ascend 950PR/Ascend 950DT，该选项默认值为disable。
<!-- end id8 -->

<!-- npu="A3" id9 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品，该选项默认值为disable。
<!-- end id9 -->

<!-- npu="910b" id10 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品，该选项默认值为disable。
<!-- end id10 -->

<!-- npu="310b" id11 -->
Atlas 200I/500 A2 推理产品，该选项默认值为enable。
<!-- end id11 -->

<!-- npu="910" id12 -->
Atlas 训练系列产品，该选项默认值为enable。
<!-- end id12 -->

<!-- npu="310p" id13 -->
Atlas 推理系列产品，该选项默认值为enable。
<!-- end id13 -->

若本参数的取值为disable，则需要安装算子二进制文件包，请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)。

## ACL\_OP\_DETERMINISTIC取值说明

是否开启确定性计算。

- 0：默认值，不开启确定性计算。配置该选项时，算子在相同的硬件和输入下，多次执行的结果可能不同。这个差异的来源，一般是因为在算子实现中，存在异步的多线程执行，会导致浮点数累加的顺序变化。
- 1：开启确定性计算。配置该选项时，算子在相同的硬件和输入下，多次执行将产生相同的输出。但启用确定性计算往往导致算子执行变慢。

通常建议不开启确定性计算，因为确定性计算往往会导致算子执行变慢，进而影响性能。当发现模型多次执行结果不同，或者是进行精度调优时，可开启确定性计算，辅助模型调试、调优。

## ACL\_CUSTOMIZE\_DTYPES取值说明

\*.cfg配置文件路径，包含文件名，配置文件中列举需要指定计算精度的算子名称或算子类型，每个算子单独一行。通过该配置，在模型编译时，可自定义某个或某些算子的计算精度。

**配置约束：**

- 路径和文件名支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、英文冒号\(:\)、中文字符。
- 配置文件中若为算子名称，以**Opname::InputDtype:dtype1,...,OutputDtype:dtype1,...**格式进行配置，每个Opname单独一行，dtype1，dtype2..需要与可设置计算精度的算子输入，算子输出的个数一一对应**。**
- 配置文件中若为算子类型，以**OpType::TypeName:InputDtype:dtype1,...,OutputDtype:dtype1,...**格式进行配置，每个OpType单独一行，dtype1，dtype2..需要与可设置计算精度的算子输入，算子输出的个数一一对应，且算子OpType必须为基于Ascend IR定义的算子的OpType，OpType查看方法请参见【《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)中的“FAQ \> 如何确定原始框架网络模型中的算子与AI处理器支持的算子的对应关系”。
- 对于同一个算子，如果同时配置了**Opname**和**OpType**的配置项，编译时以**Opname**的配置项为准。
- 使用该参数指定某个算子的计算精度时，如果模型转换过程中该算子被融合掉，则该算子指定的计算精度不生效。

## ACL\_OP\_PRECISION\_MODE

设置算子精度模式的配置文件（.ini格式）路径以及文件名，路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符.

配置文件中支持设置如下精度模式：

- high\_precision：表示高精度。
- high\_performance：表示高性能。
    <!-- npu="A3,910b" id14 -->
- enable\_float\_32\_execution：算子内部处理时使用FP32数据类型功能，该场景下FP32数据类型不会自动转换为HF32数据类型；若使用HF32计算，精度损失超过预期时，可启用该配置，指定部分算子内部计算时使用FP32，保持精度。

    **该选项仅Atlas A2 训练系列产品/Atlas A2 推理系列产品、Atlas A3 训练系列产品/Atlas A3 推理系列产品支持。**

    <!-- end id14 -->
- enable\_hi\_float\_32\_execution：算子内部处理时使用HF32数据类型功能，启用此配置后，FP32数据类型自动转换为HF32数据类型；该机制可以降低数据所占空间大小，实现性能提升。

    **当前版本该选项暂不支持。**

- support\_out\_of\_bound\_index：表示对gather、scatter和segment类算子的indices输入进行越界校验，校验会降低算子的执行性能。
- keep\_fp16：算子内部处理时使用FP16数据类型功能，该场景下FP16数据类型不会自动转换为FP32数据类型；若使用FP32计算时性能不满足预期，同时精度要求不高情况下，可以选择keep\_fp16模式，**牺牲精度提升性能，不建议使用该低精度模式**。
- super\_performance：表示超高性能，和高性能相比，在算法计算公式上进行了优化。

构造算子精度模式配置文件_op\_precision.ini_，并在该文件中按照算子类型、节点名称设置精度模式，每一行设置一个算子类型或节点名称的精度模式，按节点名称设置精度模式的优先级高于按算子类型。

配置样例如下：

```json
[ByOpType]
optype1=high_precision
optype2=high_performance
optype3=support_of_bound_index

[ByNodeName]
nodename1=high_precision
nodename2=high_performance
nodename3=support_of_bound_index
```

## ACL\_ALLOW\_HF32取值说明

算子内部计算时是否允许HF32类型替换FP32类型，true允许，false不允许，当前版本该配置仅针对Conv类算子与Matmul类算子生效。默认针对Conv类算子，启用FP32转换为HF32；默认针对Matmul类算子，关闭FP32转换为HF32。**预留参数，当前版本暂不支持**。

HF32是AI处理器推出的专门用于算子内部计算的单精度浮点类型，与其他常用数据类型的比较如下图所示。HF32与FP32支持相同的数值范围，但尾数位精度（11位）却接近FP16（10位）。通过降低精度让HF32单精度数据类型代替原有的FP32单精度数据类型，可大大降低数据所占空间大小，实现性能的提升。

![](figures/zh-cn_image_0000002550155921.png)

## ACL\_PRECISION\_MODE\_V2取值说明

网络模型的算子精度模式，如果不配置该编译选项，默认采用fp16。

相比ACL\_PRECISION\_MODE选项，ACL\_PRECISION\_MODE\_V2是新版本中新增的，精度模式选项值语义更清晰，便于理解。

- **fp16（默认值）：**

    表示原图中算子精度为float16、bfloat16或float32时，强制选择float16。

- **origin：**

    保持原图精度。

    - 如果原图中某算子精度为float16，AI Core中该算子的实现不支持float16、仅支持float32和bfloat16，则系统内部会自动采用高精度float32。
    - 如果原图中某算子精度为float16，AI Core中该算子的实现不支持float16、仅支持bfloat16，则会使用float16的AI CPU算子；如果AI CPU算子也不支持，则执行报错。
    - 如果原图中某算子精度为float32，AI Core中该算子的实现不支持float32类型、仅支持float16类型，则会使用float32的AI CPU算子；如果AI CPU算子也不支持，则执行报错。

- **cube\_fp16in\_fp32out：**

    AI Core中该算子既支持float32又支持float16数据类型时，系统内部根据算子类型不同，选择不同的处理方式。

    - 对于矩阵计算类算子，系统内部会按算子实现的支持情况处理：
        1. 优先选择输入数据类型为float16且输出数据类型为float32；
        2. 如果1中的场景不支持，则选择输入数据类型为float32且输出数据类型为float32；
        3. 如果2中的场景不支持，则选择输入数据类型为float16且输出数据类型为float16；
        4. 如果3中的场景不支持，则报错。

    - 对于矢量计算类算子，表示原图中算子精度为float16或bfloat16，强制选择float32。

        如果原图中存在部分算子，在AI Core中该算子的实现不支持float32，比如某算子仅支持float16类型，则该参数不生效，仍然使用支持的float16；如果在AI Core中该算子的实现不支持float32，且又配置了黑名单（precision\_reduce = false），则会使用float32的AI CPU算子；如果AI CPU算子也不支持，则执行报错。

- **mixed\_float16：**

    表示使用混合精度float16、bfloat16和float32数据类型来处理神经网络。针对原图中float32和bfloat16数据类型的算子，按照内置的优化策略，自动将部分float32和bfloat16的算子降低精度到float16，从而在精度损失很小的情况下提升系统性能并减少内存使用。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

    - 若取值为true（白名单），则表示允许将当前float32和bfloat16类型的算子，降低精度到float16。
    - 若取值为false（黑名单），则不允许将当前float32和bfloat16类型的算子降低精度到float16，相应算子仍旧使用float32或bfloat16精度。
    - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

    <!-- npu="950,A3,910b,310b" id15 -->
- **mixed\_bfloat16：**

    表示使用混合精度bfloat16和float32数据类型来处理神经网络。针对原图中float32数据类型的算子，按照内置的优化策略，自动将部分float32的算子降低精度到bfloat16，从而在精度损失很小的情况下提升系统性能并减少内存使用；如果算子不支持bfloat16和float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

    - 若取值为true（白名单），则表示允许将当前float32类型的算子，降低精度到bfloat16。
    - 若取值为false（黑名单），则不允许将当前float32类型的算子降低精度到bfloat16，相应算子仍旧使用float32精度。
    - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。
    <!-- end id15 -->

    <!-- npu="950" id16 -->
- **mixed\_hif8：**

    开启自动混合精度功能，表示混合使用hifloat8（此数据类型介绍可参见[Link](https://arxiv.org/abs/2409.16626?context=cs.AR)）、float16、bfloat16和float32数据类型来处理神经网络。针对原图中float16、bfloat16和float32数据类型的算子，按照内置的优化策略，自动将部分float16、bfloat16和float32的算子降低精度到hifloat8，从而在精度损失很小的情况下提升系统性能并减少内存使用。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“**precision\_reduce**”参数的取值：

    - 若取值为true（白名单），则表示允许将当前float16、bfloat16和float32类型的算子，降低精度到hifloat8。
    - 若取值为false（黑名单），则不允许将当前float16、bfloat16和float32类型的算子降低精度到hifloat8，相应算子仍旧使用float16、bfloat16或float32精度。
    - 若原图中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

- **cube\_hif8：**

    表示若原图中的cube算子既支持hifloat8，又支持float16、bfloat16或float32数据类型时，强制选择hifloat8数据类型。
    <!-- end id16 -->

## ACL\_OP\_DEBUG\_OPTION取值说明

当前仅支持配置为oom，表示开启算子编译阶段的Global Memory访问越界检测。

编译算子前调用aclSetCompileopt接口将ACL\_OP\_DEBUG\_OPTION配置为oom，同时配合调用aclrtCtxSetSysParamOpt接口（作用域是Context）或aclrtSetSysParamOpt接口（作用域是进程）将ACL\_OPT\_ENABLE\_DEBUG\_KERNEL配置为1，开启Global Memory访问越界检测，这时执行算子过程中，若从Global Memory中读写数据（例如读算子输入数据、写算子输出数据等）出现内存越界，则会返回“EZ9999”错误码，表示存在算子AI Core Error问题。
