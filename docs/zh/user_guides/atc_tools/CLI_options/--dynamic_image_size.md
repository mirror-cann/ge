# --dynamic\_image\_size

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
全量芯片支持
<!-- end id2 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_image_size_res.md#id1 -->

<!-- npu="IPV350" id1 -->
IPV350：不支持
<!-- end id1 -->

## 功能说明

设置输入图片的动态分辨率参数。适用于执行推理时，每次处理图片宽和高不固定的场景。

## 关联参数

- 该参数需要与[--input\_shape](--input_shape.md)配合使用，不能与[--dynamic\_batch\_size](--dynamic_batch_size.md)、[--dynamic\_dims](--dynamic_dims.md)同时使用。
- 使用该参数设置动态分辨率时，[--input\_format](--input_format.md)参数只支持配置为NCHW、NHWC；其他format场景下，设置分辨率请使用[--dynamic\_dims](--dynamic_dims.md)参数。

## 参数取值

**参数值：**
动态分辨率参数，例如"imagesize1\_height,imagesize1\_width;imagesize2\_height,imagesize2\_width"。

**参数值格式：**
指定的参数必须放在双引号中，档位之间英文**分号**分隔，每档内参数使用英文**逗号**分隔。

**参数值约束：**

<!-- npu="A3,910b,910,310p,310b" id3 -->
- 针对如下产品，档位数约束为：档位数取值范围为\(1,100\]，即必须设置至少2个档位，最多支持100档配置。

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
- 针对Ascend 950PR/Ascend 950DT，档位数约束为：档位数取值范围为\(1, 256\]，即必须设置至少2个档位，最多支持256档配置。
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--dynamic_image_size_res.md#id2 -->

- 如果用户设置的分辨率数值过大或档位过多，在运行环境执行推理时，建议执行**swapoff -a**命令关闭swap交换区间作为内存的功能，防止出现由于内存不足，将swap交换空间作为内存继续调用，导致运行环境异常缓慢的情况。

## 推荐配置及收益

如果用户设置的分辨率数值过大或档位过多，可能会导致模型转换失败，此时建议用户减少档位或调低档位数值。

## 示例

```bash
atc --input_shape="data:8,3,-1,-1;img_info:8,4,-1,-1"  --dynamic_image_size="416,416;832,832" ...
```

其中，“--input\_shape”中的“-1”表示设置动态分辨率。则ATC在编译模型时，支持的输入组合档数分别为：

第0档：data\(8,3,416,416\)+img\_info\(8,4,416,416\)

第1档：data\(8,3,832,832\)+img\_info\(8,4,832,832\)

## 依赖约束

- **使用约束：**
  - 不支持含有过程动态shape算子（网络中间层shape不固定）的网络。
  - 如果用户设置了动态分辨率，则请确保不同档位的分辨率能在原生框架下正常推理。
  - 如果用户设置了动态分辨率，实际推理时，使用的数据集图片大小需要与具体使用的分辨率相匹配。
  - 如果用户设置了动态分辨率，即输入图片的宽和高不确定，同时又通过[--insert\_op\_conf](--insert_op_conf.md)参数设置了静态AIPP功能：该场景下，AIPP配置文件中不能开启Crop和Padding功能，并且需要将配置文件中的src\_image\_size\_w和src\_image\_size\_h取值设置为0。
  - 如果用户设置了动态分辨率，同时又通过[--insert\_op\_conf](--insert_op_conf.md)参数设置了动态AIPP功能：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口，设置动态AIPP相关参数值时，不能开启Crop和Padding功能。该场景下，还需要确保通过aclmdlSetInputAIPP接口设置的宽和高与[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口设置的宽、高相等，都必须设置成动态分辨率最大档位的宽、高。

  - 通过该参数设置动态分辨率特性后，生成的离线模型网络结构会与固定分辨率场景下的不同，推理性能可能存在差异。

- **接口约束：**

    如果模型转换时通过该参数设置了动态分辨率，则使用应用工程进行模型推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口，用于设置真实的分辨率，且实际推理时，使用的数据集图片大小需要与具体使用的分辨率相匹配。
  - 不使用[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口，则模型执行时，默认按照动态分辨率设置范围的最大档位宽、高进行赋值。
