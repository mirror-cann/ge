# 精度调优

## ge.customizeDtypes

通过该参数自定义模型编译时算子的计算精度，模型中其他算子以ge.exec.precision\_mode或ge.exec.precision\_mode\_v2指定的精度模式进行编译。该参数需要配置为配置文件路径及文件名，例如：/home/test/customize\_dtypes.cfg。

**参数值约束：**

- 配置文件中列举需要自定义计算精度的算子名称或算子类型，每个算子单独一行，且算子类型必须为基于IR定义的算子的类型。
- 对于同一个算子，如果同时配置了算子名称和算子类型，编译时以算子名称为准。
- 指定某个算子的计算精度时，如果算子编译过程中该算子被融合掉，则该算子指定的计算精度不生效。

配置文件格式要求：

```text
# 按照算子名称配置
Opname1::InputDtype:dtype1,dtype2,…OutputDtype:dtype1,…
Opname2::InputDtype:dtype1,dtype2,…OutputDtype:dtype1,…
# 按照算子类型配置
OpType::TypeName1:InputDtype:dtype1,dtype2,…OutputDtype:dtype1,…
OpType::TypeName2:InputDtype:dtype1,dtype2,…OutputDtype:dtype1,…
```

配置文件配置示例：

```text
# 按照算子名称配置
resnet_v1_50/block1/unit_3/bottleneck_v1/Relu::InputDtype:float16,int8,OutputDtype:float16,int8
# 按照算子类型配置
OpType::Relu:InputDtype:float16,int8,OutputDtype:float16,int8
```

> [!NOTE]说明
>
>- 算子具体支持的计算精度可以从算子信息库中查看，默认存储路径为：`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/_xxx_/aic-xxx-ops-info-*.json`，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。xxx请根据实际产品进行选择。
>- 通过该参数指定的优先级高，因此可能会导致精度/性能的下降，如果指定的dtype不支持，会导致编译失败。

**必选/可选**：可选

**生效级别**：session

## ge.exec.modify\_mixlist

混合精度场景下，通过此参数指定混合精度黑白灰名单的路径以及文件名，自行指定哪些算子允许降精度，哪些算子不允许降精度。配置为路径以及文件名，文件为JSON格式。

黑白灰名单，可从`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数下的flag参数值（其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。_xxx_请根据实际产品进行选择。）：

- 若取值为true（白名单），表示混合精度模式下，**允许**降低精度。
- 若取值为false（黑名单），表示混合精度模式下，**不允许**降低精度。
- 不配置该参数（灰名单），表示混合精度模式下，当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

**配置示例：**

```json
{"ge.exec.modify_mixlist", "/home/test/ops_info.json"};
```

ops\_info.json中可以指定算子类型，多个算子使用英文逗号分隔，样例如下：

```json
{
  "black-list": {                  // 黑名单
     "to-remove": [                // 黑名单算子转换为灰名单算子
     "Xlog1py"
     ],
     "to-add": [                   // 白名单或灰名单算子转换为黑名单算子
     "Matmul",
     "Cast"
     ]
  },
  "white-list": {                  // 白名单
     "to-remove": [                // 白名单算子转换为灰名单算子
     "Conv2D"
     ],
     "to-add": [                   // 黑名单或灰名单算子转换为白名单算子
     "Bias"
     ]
  }
}
```

上述配置文件样例中展示的算子仅作为参考，请基于实际硬件环境和具体的算子内置优化策略进行配置。黑白灰名单查询样例如下：

```json
"Conv2D":{
    "precision_reduce":{
        "flag":"true"
     }
},
```

true：白名单；false：黑名单；不配置：灰名单。

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.exec.precision\_mode

算子精度模式，配置要求为string类型。不能与ge.exec.precision\_mode\_v2同时使用，建议使用ge.exec.precision\_mode\_v2参数。

**参数取值：**

- **force\_fp32/cube\_fp16in\_fp32out：**

    配置为force\_fp32或cube\_fp16in\_fp32out，效果等同，该选项用来表示AI Core中该算子既支持float32又支持float16数据类型时，系统内部都会根据算子类型不同，选择不同的处理方式。cube\_fp16in\_fp32out为新版本中新增的，对于矩阵计算类算子，该选项语义更清晰。

  - 对于矩阵计算类算子，系统内部会按算子实现的支持情况处理：
        1. 优先选择输入数据类型为float16且输出数据类型为float32；
        2. 如果1中的场景不支持，则选择输入数据类型为float32且输出数据类型为float32；
        3. 如果2中的场景不支持，则选择输入数据类型为float16且输出数据类型为float16；
        4. 如果3中的场景不支持，则报错。

  - 对于矢量计算类算子，表示原图中算子精度为float16或bfloat16，强制选择float32。

    如果原图中存在部分算子，在AI Core中该算子的实现不支持float32，比如某算子仅支持float16类型，则该参数不生效，仍然使用支持的float16；如果在AI Core中该算子的实现不支持float32，且又配置了黑名单（precision\_reduce = false），则会使用float32的AI CPU算子；如果AI CPU算子也不支持，则执行报错。

- **force\_fp16：**

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

- **allow\_mix\_precision\_bf16：**

    表示使用混合精度bfloat16和float32数据类型来处理神经网络的过程。针对原始模型中float32数据类型的算子，按照内置的优化策略，自动将部分float32的算子降低精度到bfloat16，从而在精度损失很小的情况下提升系统性能并减少内存使用；如果AI Core中算子不支持bfloat16和float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

    若配置了该种模式，则可以在`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/config/xxx/aic-xxx-ops-info-*.json`内置优化策略文件中查看“precision\_reduce”参数的取值：

  - 若取值为true（白名单），则表示允许将当前float32类型的算子，降低精度到bfloat16。
  - 若取值为false（黑名单），则不允许将当前float32类型的算子降低精度到bfloat16，相应算子仍旧使用float32精度。
  - 若网络模型中算子没有配置该参数（灰名单），当前算子的混合精度处理机制和前一个算子保持一致，即如果前一个算子支持降精度处理，当前算子也支持降精度；如果前一个算子不允许降精度，当前算子也不支持降精度。

- **allow\_fp32\_to\_bf16：**
  - 如果原图中算子精度为float32，则优先使用原图精度float32，如果AI Core中算子不支持float32，则降低精度到bfloat16；如果AI Core中算子不支持bfloat16，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。
  - 如果原图中算子精度为bfloat16，则优先使用原图精度bfloat16，如果AI Core中算子不支持bfloat16，则选择float32，如果AI Core中算子不支持float32，则使用AI CPU算子进行计算；如果AI CPU算子也不支持，则执行报错。

上述路径中的$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。_xxx_请根据实际产品进行选择。

**参数默认值：**

<!-- npu="910" id1 -->
训练场景下，针对Atlas 训练系列产品，参数默认值为“allow\_fp32\_to\_fp16”。
<!-- end id1 -->

<!-- npu="910b" id2 -->
训练场景下，针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，参数默认值为“must\_keep\_origin\_dtype”。
<!-- end id2 -->

<!-- npu="A3" id3 -->
训练场景下，针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，参数默认值为“must\_keep\_origin\_dtype”。
<!-- end id3 -->

<!-- npu="950" id4 -->
训练场景下，针对Ascend 950PR/Ascend 950DT，参数默认值为“must\_keep\_origin\_dtype”。
<!-- end id4 -->

在线推理场景下，参数默认值为“force\_fp16”。

**参数值约束：**

<!-- npu="950,A3,910b" id5 -->
**bfloat16数据类型仅支持以下产品**：

Atlas A2 训练系列产品/Atlas A2 推理系列产品

Atlas A3 训练系列产品/Atlas A3 推理系列产品

Ascend 950PR/Ascend 950DT
<!-- end id5 -->

**配置示例：**

```c++
{"ge.exec.precision_mode", "force_fp16"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.exec.precision\_mode\_v2

算子精度模式，配置要求为string类型。不能与ge.exec.precision\_mode同时使用，建议使用ge.exec.precision\_mode\_v2参数。

**参数取值：**

- **fp16**：

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

上述路径中的$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。_xxx_请根据实际产品进行选择。

**参数默认值：**

<!-- npu="910" id6 -->
训练场景下，针对Atlas 训练系列产品，该配置项无默认取值，以“precision\_mode”参数的默认值为准，即“allow\_fp32\_to\_fp16”。
<!-- end id6 -->

<!-- npu="910b" id7 -->
训练场景下，针对Atlas A2 训练系列产品/Atlas A2 推理系列产品，参数默认值为“origin”。
<!-- end id7 -->

<!-- npu="A3" id8 -->
训练场景下，针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，参数默认值为“origin”。
<!-- end id8 -->

<!-- npu="950" id9 -->
训练场景下，针对Ascend 950PR/Ascend 950DT，参数默认值为“origin”。
<!-- end id9 -->

在线推理场景下，参数默认值为“fp16”。

<!-- npu="950,A3,910b" id10 -->
**参数值约束：**

**bfloat16数据类型仅支持以下产品**：

Atlas A2 训练系列产品/Atlas A2 推理系列产品

Atlas A3 训练系列产品/Atlas A3 推理系列产品

Ascend 950PR/Ascend 950DT
<!-- end id10 -->

**配置示例：**

```c++
{"ge.exec.precision_mode_v2", "fp16"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph
