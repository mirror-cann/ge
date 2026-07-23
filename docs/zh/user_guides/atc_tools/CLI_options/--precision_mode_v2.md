# --precision\_mode\_v2

## 产品支持情况

全量芯片支持

## 功能说明

设置网络模型的精度模式。

## 关联参数

- 该参数不能与[--precision\_mode](--precision_mode.md)参数同时使用，建议使用[--precision\_mode\_v2](--precision_mode_v2.md)参数，--precision\_mode\_v2是新版本中新增的，选项值语义更清晰，便于理解。
- 当取值为**mixed\_float16**或者**mixed\_bfloat16**或**mixed\_hif8**时，如果用户想要在内置优化策略基础上进行调整，自行指定哪些算子允许降精度，哪些算子不允许降精度，则需要参见[--modify\_mixlist](--modify_mixlist.md)参数设置。
- 推理场景下，使用[--precision\_mode\_v2](--precision_mode_v2.md)参数设置整个网络模型的精度模式，可能会有个别算子存在性能或精度问题，该场景下可以使用[--keep\_dtype](--keep_dtype.md)参数，使原始网络模型编译时保持个别算子的计算精度不变，但[--precision\_mode\_v2](--precision_mode_v2.md)参数取值为**origin**时，[--keep\_dtype](--keep_dtype.md)不生效。

## 参数取值

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

- **mixed\_bfloat16：**

    表示使用混合精度bfloat16和float32数据类型来处理神经网络。针对原图中float32数据类型的算子，按照内置的优化策略，自动将部分float32的算子降低精度到bfloat16，从而在精度损失很小的情况下提升系统性能并减少内存使用；如果算子不支持bfloat16和float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

  - 若取值为true（白名单），则表示允许将当前float32类型的算子，降低精度到bfloat16。
  - 若取值为false（黑名单），则不允许将当前float32类型的算子降低精度到bfloat16，相应算子仍旧使用float32精度。
  - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

- **mixed\_hif8：**

    开启自动混合精度功能，表示混合使用hifloat8（此数据类型介绍可参见[Link](https://arxiv.org/abs/2409.16626?context=cs.AR)）、float16、bfloat16和float32数据类型来处理神经网络。针对原图中float16、bfloat16和float32数据类型的算子，按照内置的优化策略，自动将部分float16、bfloat16和float32的算子降低精度到hifloat8，从而在精度损失很小的情况下提升系统性能并减少内存使用。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“**precision\_reduce**”参数的取值：

  - 若取值为true（白名单），则表示允许将当前float16、bfloat16和float32类型的算子，降低精度到hifloat8。
  - 若取值为false（黑名单），则不允许将当前float16、bfloat16和float32类型的算子降低精度到hifloat8，相应算子仍旧使用float16、bfloat16或float32精度。
  - 若原图中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

- **cube\_hif8：**

    表示若原图中的cube算子既支持hifloat8，又支持float16、bfloat16或float32数据类型时，强制选择hifloat8数据类型。

上述路径中的$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。xxx请根据实际产品进行选择。

**参数值约束：**

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--precision_mode_v2_res.md#id1 -->

<!-- npu="IPV350" id9 -->
- **IPV350不支持bfloat16、hif8类型，对应选项也不支持。**
<!-- end id9 -->

<!-- npu="950,A3,910b,310b" id8 -->
- **bfloat16数据类型仅支持以下产品型号**：

    <!-- npu="910b" id1 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id1 -->

    <!-- npu="A3" id2 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id2 -->

    <!-- npu="310b" id3 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id3 -->

    <!-- npu="950" id4 -->
    Ascend 950PR/Ascend 950DT
    <!-- end id4 -->
<!-- end id8 -->

<!-- npu="950" id5 -->
- **hif8数据类型仅支持以下产品型号**：

    Ascend 950PR/Ascend 950DT

<!-- end id5 -->
- 该参数默认为性能优先，后续推理时可能会导致精度溢出问题。如果推理时出现精度问题，可以参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理 \> 精度/性能优化 \> 模型推理精度提升建议”进行定位。
- 如果用户聚焦精度问题，可以修改为其他取值，比如**origin。**

## 推荐配置及收益

所配置的精度模式不同，网络模型精度以及性能有所不同，具体为：

精度高低排序：origin\>mixed\_float16\>fp16\>mixed\_bfloat16\>mixed\_hif8；性能优劣排序：mixed\_hif8\>mixed\_bfloat16\>fp16\>=mixed\_float16\>origin

## 示例

```bash
atc --precision_mode_v2=fp16 ...
```

## 使用约束

混合精度场景下，如果版本升级后出现推理性能下降，建议使用AOE工具重新进行调优，调优完成后，通过[--op\_bank\_path](--op_bank_path.md)参数加载算子调优后自定义知识库的路径，然后重新进行模型转换。

算子调优详情请参见[《AOE调优工具》](https://hiascend.com/document/redirect/CannCommunityToolAoe)。
