# --dynamic\_batch\_size

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
全量芯片支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_batch_size_res.md#id1 -->

<!-- npu="IPV350" id2 -->
IPV350：不支持
<!-- end id2 -->

## 功能说明

设置动态batch\_size参数，适用于执行推理时，每次处理图片或者句子数量不固定的场景。

在某些推理场景，如检测出目标后再执行目标识别网络，由于目标个数不固定导致目标识别网络输入batch\_size不固定。如果每次推理都按照最大的batch\_size或最大分辨率进行计算，会造成计算资源浪费。因此，推理需要支持动态batch\_size和动态分辨率的场景，使用ATC工具时，通过该参数设置支持的batch\_size，通过[--dynamic\_image\_size](--dynamic_image_size.md)参数设置支持的分辨率档位。

模型转换完成后，在生成的om离线模型中，会新增一个输入，在模型推理时通过该新增的输入提供具体的batch\_size值。例如，a输入的batch\_size是动态的，在om离线模型中，会有与a对应的b输入来描述a的具体batch\_size。

## 关联参数

- 该参数需要与[--input\_shape](--input_shape.md)配合使用，不能与[--dynamic\_image\_size](--dynamic_image_size.md)、[--dynamic\_dims](--dynamic_dims.md)、[--input\_shape\_range](--input_shape_range.md)同时使用。且只支持N在shape首位的场景，即shape的第一位设置为"-1"。如果N在非首位场景下，请使用[--dynamic\_dims](--dynamic_dims.md)参数进行设置。

## 参数取值

**参数值：**
档位数，例如"1,2,4,8"。

**参数值格式：**
指定的参数必须放在双引号中，档位之间使用英文逗号分隔。

**参数值约束：**

<!-- npu="A3,910b,910,310p,310b" id3 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置；每个档位数值建议限制为：\[1\~2048\]。

    <!-- npu="A3" id5 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id5 -->
    <!-- npu="910b" id6 -->
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
<!-- end id3 -->
<!-- npu="950" id4 -->
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置；每个档位数值建议限制为：\[1\~2048\]。
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_batch_size_res.md#id2 -->

- 如果用户设置的档位数值过大或档位过多，在运行环境执行推理时，建议执行**swapoff -a**命令关闭swap交换区间作为内存的功能，防止出现由于内存不足，将swap交换空间作为内存继续调用，导致运行环境异常缓慢的情况。

## 推荐配置及收益

- 如果用户设置的档位数值过大或档位过多，可能会导致模型转换失败，此时建议用户减少档位或调低档位数值。
- CV（计算机视觉）类的网络，[--dynamic\_batch\_size](--dynamic_batch_size.md)建议取值为8、16档位，该场景下的网络性能比单个batch\_size更优（8、16档位只是建议取值，实际使用时还请以实际测试结果为准）。
- OCR/NLP（文字识别/自然语言处理）类网络，[--dynamic\_batch\_size](--dynamic_batch_size.md)档位取值建议为16的整数倍（该档位值只是建议取值，实际使用时还请以实际测试结果为准）。

## 示例

```bash
atc --input_shape="data:-1,3,416,416;img_info:-1,4"  --dynamic_batch_size="1,2,4,8" ...
```

其中，“--input\_shape”中的“-1”表示设置动态batch\_size。则ATC在模型编译时，支持的输入组合档数分别为：

第0档：data\(1,3,416,416\)+img\_info\(1,4\)

第1档：data\(2,3,416,416\)+img\_info\(2,4\)

第2档：data\(4,3,416,416\)+img\_info\(4,4\)

第3档：data\(8,3,416,416\)+img\_info\(8,4\)

## 依赖约束

- **使用约束：**
  - 不支持含有过程动态shape算子（网络中间层shape不固定）的网络。
  - 如果用户设置了动态batch\_size，同时又通过[--insert\_op\_conf](--insert_op_conf.md)参数设置了动态AIPP功能：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口设置动态AIPP相关参数值时，需确保batch\_size要设置为最大Batch数。。

  - 通过该参数设置动态batch\_size特性后，生成的离线模型网络结构会与固定batch\_size场景下的不同，推理性能可能存在差异。

- **接口约束：**

    如果模型转换时通过该参数设置了动态batch\_size，则使用应用工程进行推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetDynamicBatchSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicBatchSize.md)接口，用于设置真实的batch\_size档位。
  - 不使用[aclmdlSetDynamicBatchSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicBatchSize.md)接口，则模型执行时，默认按照batch\_size设置范围的最大值进行赋值。
