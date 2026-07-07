# 动态shape

## INPUT\_SHAPE

模型输入的shape信息。此参数实际对应的options参数为`ge.inputShape`。

**参数取值**：

- 模型为固定shape，INPUT\_SHAPE参数为**可选**配置
  - 若模型为单个输入，则shape信息为"input\_name:n,c,h,w"。
  - 若模型有多个输入，则shape信息为"input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"；不同输入之间使用**英文分号**分隔，input\_name必须是转换前的网络模型中的节点名称。

- 模型为非固定shape，INPUT\_SHAPE参数**必须**配置

    若原始模型中输入数据的某个或某些维度值不固定，当前支持通过设置shape分档或设置shape范围两种方式转换模型。

  - 设置shape分档（静态shape），包括设置batch\_size档位、设置分辨率档位、设置动态维度档位。

    设置INPUT\_SHAPE参数时，将对应维度值设置为-1，同时配合使用DYNAMIC\_BATCH\_SIZE（设置batch\_size档位）或DYNAMIC\_IMAGE\_SIZE（设置分辨率档位）或DYNAMIC\_DIMS（设置动态维度档位）参数。详细用法请参考DYNAMIC\_BATCH\_SIZE、DYNAMIC\_IMAGE\_SIZE、DYNAMIC\_DIMS参数说明。

  - 设置shape范围（动态shape）。

    <!-- npu="310b" id1 -->
    Atlas 200I/500 A2 推理产品**不支持设置shape范围**。
    <!-- end id1 -->

    <!-- npu="IPV350" id2 -->
    IPV350**不支持设置shape范围**。
    <!-- end id2 -->

    设置INPUT\_SHAPE参数时，可将对应维度的值设置为范围，例如1\~10，**设置的range值范围必须有效**。

    - 支持按照name设置："input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"，例如："input\_name1:8\~20,3,5,-1;input\_name2:5,3\~9,10,-1"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称。如果用户知道data节点的name，推荐按照name设置。
    - 支持按照index设置："n1,c1,h1,w1;n2,c2,h2,w2"，例如："8\~20,3,5,-1;5,3\~9,10,-1"。可以不指定节点名，节点按照索引顺序排列，节点中间使用英文分号分隔。按照index设置shape范围时，data节点需要设置属性index，说明是第几个输入，index从0开始。

    如果用户不想指定维度的范围或具体取值，则可以将其设置为-1，表示此维度可以使用\>=0的任意取值，该场景下取值上限为int64数据类型表达范围，但受限于host和device侧物理内存的大小，用户可以通过增大内存来支持。

- 模型shape为标量
  - 非动态分档场景：

    shape为标量的输入，可选配置，例如模型有两个输入，input\_name1为标量，即shape为"\[\]"形式，input\_name2输入shape为\[n2,c2,h2,w2\]，则shape信息为"**input\_name1:**;input\_name2:n2,c2,h2,w2"，不同输入之间使用**英文分号**分隔，input\_name必须是转换前的网络模型中的节点名称；标量的输入如果配置，则配置为空。

  - 动态分档场景：

    如果模型输入中既有标量shape，又有支持动态分档的shape，则标量输入不能忽略，必须配置。例如模型有三个输入，分别为A:\[-1,c1,h1,w1\]、B:\[\]、C:\[n2,c2,h2,w2\]，则shape信息为"A:-1,c1,h1,w1;**B:**;C:n2,c2,h2,w2"，标量输入B必须配置。

**配置示例：**

- 固定shape，例如某网络的输入shape信息，输入1**：**input\_0\_0 \[16,32,208,208\]，输入2：input\_1\_0 \[16,64,208,208\]，则INPUT\_SHAPE的配置信息为：

    ```c++
    {ge::ir_option::INPUT_SHAPE, "input_0_0:16,32,208,208;input_1_0:16,64,208,208"}
    ```

- 非固定shape，静态shape：
  - 设置batch\_size档位的示例，请参见DYNAMIC\_BATCH\_SIZE。
  - 设置分辨率档位的示例，请参见DYNAMIC\_IMAGE\_SIZE。
  - 设置指定维度档位的示例，请参见DYNAMIC\_DIMS。

- 非固定shape，动态shape，设置shape范围的示例：

    ```c++
    {ge::ir_option::INPUT_SHAPE, "input_0_0:1~10,32,208,208;input_1_0:16,64,100~208,100~208"}
    ```

    详细使用示例以及使用注意事项请参见[更多特性\>动态输入shape范围](../../../../../user_guides/graph_dev/more_features/dynamic_shape.md#动态输入shape范围)。

- shape为标量
  - 非动态分档场景

    shape为标量的输入，可选配置。例如模型有两个输入，**input\_name1**为标量，input\_name2输入shape为\[16,32,208,208\]，配置示例为：

    ```c++
    {ge::ir_option::INPUT_SHAPE, "input_name1:;input_name2:16,32,208,208"}
    ```

    上述示例中的**input\_name1**为可选配置**。**

  - 动态分档场景

    shape为标量的输入，必须配置。例如模型有三个输入，shape信息分别为A:\[-1,32,208,208\]、**B:\[\]**、C:\[16,64,208,208\]，则配置示例为（A为动态分档输入，此处以设置batch\_size档位为例）：

    ```c++
    {ge::ir_option::INPUT_SHAPE, "A:-1,32,208,208;B:;C:16,64,208,208"},
    {ge::ir_option::DYNAMIC_BATCH_SIZE, "1,2,4"}
    ```

> [!NOTE]说明
>
>如果模型转换时通过该参数设置了shape的范围，使用应用工程进行模型推理时，需要在**模型执行**接口之前，调用**aclmdlSetDatasetTensorDesc**接口，用于设置真实的输入Tensor描述信息（输入shape范围）；模型执行之后，调用**aclmdlGetDatasetTensorDesc**接口获取模型动态输出的Tensor描述信息；再进一步调用**aclTensorDesc**下的操作接口获取输出Tensor数据占用的内存大小、Tensor的Format信息、Tensor的维度信息等。
>
>关于**aclmdlSetDatasetTensorDesc**、**aclmdlGetDatasetTensorDesc**等接口的具体使用方法，请参见《GE图引擎 API》。

**产品支持情况：**

全量芯片支持。

## DYNAMIC\_BATCH\_SIZE

设置动态batch档位参数，适用于执行推理时，每次处理图片数量不固定的场景。此参数实际对应的options参数为`ge.dynamicBatchSize`。

该参数需要与INPUT\_SHAPE配合使用，不能与DYNAMIC\_IMAGE\_SIZE、DYNAMIC\_DIMS同时使用；且只支持N在shape首位的场景，即shape的第一位设置为"-1"。如果N在非首位场景下，请使用DYNAMIC\_DIMS参数进行设置。

**参数取值**：档位数，例如"1,2,4,8"。

**参数值格式**：指定的参数必须放在双引号中，档位之间使用英文逗号分隔。

**参数值约束：**

<!-- npu="950" id3 -->
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置；每个档位数值建议限制为：\[1\~2048\]。
<!-- end id3 -->

<!-- npu="A3,910b,910,310p,310b" id4 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置；每个档位数值建议限制为：\[1\~2048\]。

    <!-- npu="A3" id5 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id5 -->

    <!-- npu="310b" id6 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id6 -->

    <!-- npu="310b" id7 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id7 -->

    <!-- npu="310p" id8 -->
    Atlas 推理系列产品
    <!-- end id8 -->

    <!-- npu="910" id9 -->
    Atlas 训练系列产品
    <!-- end id9 -->
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id1 -->

**配置示例：**

INPUT\_SHAPE中的“-1”表示设置动态batch。

```c++
{ge::ir_option::INPUT_FORMAT, "NHWC"},
{ge::ir_option::INPUT_SHAPE, "data:-1,3,416,416"},
{ge::ir_option::DYNAMIC_BATCH_SIZE, "1,2,4,8"}
```

详细使用示例以及使用注意事项请参见[更多特性 \> 动态BatchSize](../../../../../user_guides/graph_dev/more_features/dynamic_shape.md#动态batch_size)。

**产品支持情况：**

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id2 -->

## DYNAMIC\_IMAGE\_SIZE

设置输入图片的动态分辨率参数。适用于执行推理时，每次处理图片宽和高不固定的场景。此参数实际对应的options参数为`ge.dynamicImageSize`。

该参数需要与INPUT\_SHAPE配合使用，不能与DYNAMIC\_BATCH\_SIZE、DYNAMIC\_DIMS同时使用。

**参数取值：**  "imagesize1\_height,imagesize1\_width;imagesize2\_height,imagesize2\_width"。

**参数值格式：**指定的参数必须放在双引号中，档位之间英文**分号**分隔，每档内参数使用英文**逗号**分隔。

**参数值约束：**

<!-- npu="950" id10 -->
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置。
<!-- end id10 -->

<!-- npu="A3,910b,910,310p,310b" id11 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置。

    <!-- npu="A3" id12 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id12 -->

    <!-- npu="910b" id13 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id13 -->

    <!-- npu="310b" id14 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id14 -->

    <!-- npu="310p" id15 -->
    Atlas 推理系列产品
    <!-- end id15 -->

    <!-- npu="910" id16 -->
    Atlas 训练系列产品
    <!-- end id16 -->
<!-- end id11 -->

<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id3 -->

**配置示例：**

INPUT\_SHAPE中的“-1”表示设置动态分辨率。

```c++
{ge::ir_option::INPUT_FORMAT, "NCHW"},
{ge::ir_option::INPUT_SHAPE, "data:8,3,-1,-1"},
{ge::ir_option::DYNAMIC_IMAGE_SIZE, "416,416;832,832"}
```

详细使用示例以及使用注意事项请参见[更多特性 \> 动态分辨率](../../../../../user_guides/graph_dev/more_features/dynamic_shape.md#动态分辨率)。

**产品支持情况：**

<!-- npu="950" id17 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id17 -->
<!-- npu="A3" id18 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id18 -->
<!-- npu="910b" id19 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id19 -->
<!-- npu="310b" id20 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id20 -->
<!-- npu="310p" id21 -->
- Atlas 推理系列产品：支持
<!-- end id21 -->
<!-- npu="910" id22 -->
- Atlas 训练系列产品：支持
<!-- end id22 -->
<!-- npu="IPV350" id23 -->
- IPV350：不支持
<!-- end id23 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id4 -->

## DYNAMIC\_DIMS

设置ND格式下动态维度的档位。适用于执行推理时，每次处理任意维度的场景。此参数实际对应的options参数为`ge.dynamicDims`。

该参数需要与INPUT\_SHAPE配合使用，不能与DYNAMIC\_BATCH\_SIZE、DYNAMIC\_IMAGE\_SIZE同时使用。

**参数取值**：通过"dim1,dim2,dim3;dim4,dim5,dim6;dim7,dim8,dim9"的形式设置。

**参数值格式**：所有档位必须放在双引号中，档位之间使用英文**分号**分隔，每档内参数使用英文**逗号**分隔；每档中的dim值与INPUT\_SHAPE参数中的-1标识的参数依次对应，INPUT\_SHAPE参数中有几个-1，则每档必须设置几个维度。

**参数值约束：**

<!-- npu="950" id24 -->
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置，建议配置为3\~4档。
<!-- end id24 -->

<!-- npu="A3,910b,910,310p,310b" id25 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置，建议配置为3\~4档。

    <!-- npu="A3" id26 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id26 -->

    <!-- npu="910b" id27 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id27 -->

    <!-- npu="310b" id28 -->
    Atlas 200I/500 A2 推理产品
    <!-- end id28 -->

    <!-- npu="310p" id29 -->
    Atlas 推理系列产品
    <!-- end id29 -->

    <!-- npu="910" id30 -->
    Atlas 训练系列产品
    <!-- end id30 -->

<!-- end id25 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id5 -->

**配置示例：**

```c++
{ge::ir_option::INPUT_FORMAT, "ND"},
{ge::ir_option::INPUT_SHAPE, "data:1,-1"},
{ge::ir_option::DYNAMIC_DIMS, "4;8;16;64"}
// 模型编译时，支持的data算子的shape为1,4; 1,8; 1,16;1,64
{ge::ir_option::INPUT_FORMAT, "ND"},
{ge::ir_option::INPUT_SHAPE, "data:1,-1,-1"},
{ge::ir_option::DYNAMIC_DIMS, "1,2;3,4;5,6;7,8"}
// 模型编译时，支持的data算子的shape为1,1,2; 1,3,4; 1,5,6; 1,7,8
```

详细使用示例以及使用注意事项请参见[更多特性 \> 动态维度](../../../../../user_guides/graph_dev/more_features/dynamic_shape.md#动态维度)。

**产品支持情况：**

<!-- npu="950" id31 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id31 -->
<!-- npu="A3" id32 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id32 -->
<!-- npu="910b" id33 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id33 -->
<!-- npu="310b" id34 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id34 -->
<!-- npu="310p" id35 -->
- Atlas 推理系列产品：支持
<!-- end id35 -->
<!-- npu="910" id36 -->
- Atlas 训练系列产品：支持
<!-- end id36 -->
<!-- npu="IPV350" id37 -->
- IPV350：不支持
<!-- end id37 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id6 -->

## AC\_PARALLEL\_ENABLE

<!-- npu="950,A3,910b,910,310p" id38 -->
动态shape图中，是否允许AI CPU算子和AI Core算子并行运行。此参数实际对应的options参数为`ac_parallel_enable`。

动态shape图中，开关开启时，系统自动识别图中可以和AI Core并发的AI CPU算子，不同引擎的算子下发到不同流上，实现多引擎间的并行，从而提升资源利用效率和动态shape执行性能。

**参数取值：**

- 1：允许AI CPU和AI Core算子间的并行运行。
- 0（默认）：AI CPU算子不会单独分流。

**配置示例：**

```c++
{ge::ir_option::AC_PARALLEL_ENABLE, "1"}
```
<!-- end id38 -->

<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id7 -->

**产品支持情况：**

<!-- npu="950" id39 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id39 -->
<!-- npu="A3" id40 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id40 -->
<!-- npu="910b" id41 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id41 -->
<!-- npu="310b" id42 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id42 -->
<!-- npu="310p" id43 -->
- Atlas 推理系列产品：支持
<!-- end id43 -->
<!-- npu="910" id44 -->
- Atlas 训练系列产品：支持
<!-- end id44 -->
<!-- npu="IPV350" id45 -->
- IPV350：不支持
<!-- end id45 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/dynamic_shape_res.md#id8 -->
