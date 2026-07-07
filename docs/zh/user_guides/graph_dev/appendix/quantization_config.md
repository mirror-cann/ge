# 量化相关配置

## 支持量化的层及约束

本章节给出不同框架可量化的层以及相关约束。

总体说明:

- 若网络模型输入数据类型或权重数据类型为Float16或混合精度类型（Float32/Float16共存），会关闭如下算子的量化功能：
 AvgPool、Pooling、AvgPoolV2、MaxPool、MaxPoolV3、Add、Eltwise、BatchMatMulV2（两路输入都为变量tensor）。
<!-- npu="IPV350" id1 -->
- INT16数据量化过程中，发现整网精度下降，可以通过精度比对工具，逐层比对原始模型和量化后模型输出误差（例如以余弦相似度作为标准，需要相似度达到0.99以上），找到误差较大的层，然后通过简易配置文件中的**dst\_type**参数将该层修改为INT8量化，重新进行量化。
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quantization_config_res.md#id2 -->

<!-- npu="A3,910b,IPV350" id2 -->
- **Caffe框架在如下产品形态已不演进，不保证功能可用：**

    <!-- npu="A3" id4 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id4 -->

    <!-- npu="910b" id3 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id3 -->

    <!-- npu="IPV350" id6 -->
    IPV350
    <!-- end id6 -->

<!-- end id2 -->

### 全量化

#### 均匀量化支持的层及约束（Caffe框架）

|支持的层类型|约束|**对应Ascend IR定义的层**类型|
|--|--|--|
|InnerProduct：全连接层|transpose属性为false，axis为1|FullyConnection|
|Convolution：卷积层|filter维度为4|Conv2D|
|Deconvolution：反卷积层|dilation为1、filter维度为4|Deconvolution|
|Pooling|- mode为1，全量化（weight+tensor），global_pooling为false，不支持移位N操作<br>- mode为0，只做tensor量化|Pooling|
|Eltwise|只做tensor量化且operation=1|Eltwise|

#### 均匀量化支持的层及约束（TensorFlow框架）

<!-- npu="IPV350" id9 -->
如下表格不适用于IPV350，其约束请参见[IPV350支持的层及约束](#均匀量化支持的层及约束-ipv350)。
<!-- end id9 -->

|支持的层类型|约束|**对应Ascend IR定义的层**类型|
|--|--|--|
|MatMul：全连接层|- transpose_a为False，transpose_b为False，adjoint_a为False，adjoint_b为False<br>- weight的输入来源不含有placeholder等可动态变化的节点|MatMulV2|
|Conv2D：卷积层|weight的输入来源不含有placeholder等可动态变化的节点|Conv2D|
|DepthwiseConv2dNative：Depthwise卷积层|weight的输入来源不含有placeholder等可动态变化的节点|DepthwiseConv2D|
|Conv2DBackpropInput|dilation为1，weight的输入来源不含有placeholder等可动态变化的节点|Conv2DBackpropInput|
|BatchMatMulV2|- adj_x=False<br>- 如果第二路输入为常量const，则要求为2维<br>- 当两路输入都为变量tensor时，仅支持INT8对称量化|BatchMatMulV2|
|AvgPool|不支持移位N操作|AvgPool|
|Conv3D|dilation_d为1|Conv3D|
|MaxPool|只做tensor量化|MaxPool、MaxPoolV3|
|Add|只做tensor量化，且只支持单路输入的量化|Add|

<!-- npu="A3,910b,310b" id15 -->
针对BatchMatMulV2量化层，两路输入都为变量tensor量化场景，只在以下产品能获得收益，其他产品量化后精度会下降：

<!-- npu="310b" id16 -->
Atlas 200I/500 A2 推理产品
<!-- end id16 -->

<!-- npu="910b" id17 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id17 -->

<!-- npu="A3" id18 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id18 -->
<!-- end id15 -->

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quantization_config_res.md#id3 -->

#### 均匀量化支持的层及约束（ONNX框架）

<!-- npu="IPV350" id8 -->
如下表格不适用于IPV350，其约束请参见[IPV350支持的层及约束](#均匀量化支持的层及约束-ipv350)。
<!-- end id8 -->

|支持的层类型|约束|**对应Ascend IR定义的层**类型|
|--|--|--|
|Conv：卷积层|- filter维度为5的情况下，要求dilation_d为1<br>- weight的输入来源不含有placeholder等可动态变化的节点|Conv2D、Conv3D|
|Gemm：广义矩阵乘|- transpose_a=false<br>- weight的输入来源不含有placeholder等可动态变化的节点|MatMulV2|
|ConvTranspose：转置卷积|- dilation为1、filter维度为4<br>- weight的输入来源不含有placeholder等可动态变化的节点|Conv2DTranspose|
|MatMul|- 如果第二路输入为常量const，则要求为2维<br>- 当两路输入都为变量tensor时，仅支持INT8对称量化<br>- weight的输入来源不含有placeholder等可动态变化的节点|BatchMatMulV2|
|AveragePool|global_pooling为false，不支持移位N操作|AvgPoolV2|
|MaxPool|只做tensor量化|MaxPool、MaxPoolV3|
|Add|只做tensor量化，且只支持单路输入的量化|Add|

<!-- npu="A3,910b,310b" id20 -->
针对MatMul量化层，两路输入都为变量tensor量化场景，只在以下产品能获得收益，其他产品量化后精度会下降：

<!-- npu="310b" id21 -->
Atlas 200I/500 A2 推理产品
<!-- end id21 -->

<!-- npu="910b" id22 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id22 -->

<!-- npu="A3" id23 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id23 -->
<!-- end id20 -->

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quantization_config_res.md#id4 -->

<!-- npu="IPV350" id7 -->
#### 均匀量化支持的层及约束 (IPV350)

|框架|支持的层类型|约束|**对应Ascend IR定义的层**类型|
|--|--|--|--|
|TensorFlow|MatMul：全连接层|- transpose_a为False, transpose_b为False，adjoint_a为False，adjoint_b为False<br>- weight的输入来源不含有placeholder等可动态变化的节点|MatMulV2|
|TensorFlow|Conv2D：卷积层|weight的输入来源不含有placeholder等可动态变化的节点|Conv2D|
|TensorFlow|Conv2DBackpropInput|dilation为1，weight的输入来源不含有placeholder等可动态变化的节点|Conv2DBackpropInput|
|TensorFlow|DepthwiseConv2dNative|weight的输入来源不含有placeholder等可动态变化的节点|DepthwiseConv2D|
|ONNX|ConvTranspose|- dilation为1、filter维度为4<br>- weight的输入来源不含有placeholder等可动态变化的节点|Conv2DTranspose|
<!-- end id7 -->

### 仅权重量化

<!-- npu="IPV350" id11 -->
**该版本不支持仅权重量化特性。**
<!-- end id11 -->

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quantization_config_res.md#id1 -->

<!-- npu="910b,310p" id12 -->
**仅权重量化特性，仅支持以下产品类型：**

<!-- npu="310p" id13 -->
Atlas 推理系列产品
<!-- end id13 -->

<!-- npu="910b" id14 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id14 -->
<!-- end id12 -->

**表 2**  仅权重量化场景支持的层及约束

|**Ascend IR定义的层**类型|仅权重量化权重ARQ中channel_wise=true|仅权重量化权重ARQ中asymmetric=true或false|权重和数据都量化权重ARQ中channel_wise=true|权重和数据都量化权重ARQ中asymmetric=true|约束|
|--|--|--|--|--|--|
|MatMulV2|√|true|×|×|第二路的输入来源不含有placeholder等可动态变化的节点。|
|BatchMatMulV2|√|true|×|×|第二路的输入来源不含有placeholder等可动态变化的节点。|
|FFN|√|true和false|×|×|- FFN算子的expert_tokens输入不为空。<br>- FFN算子的两个权重为Float16的常量。<br>- FFN算子的antiquant_scale1、antiquant_scale2、antiquant_offset1、antiquant_offset2四个输入为空。<br>- 权重不支持权重共享。|

其中：

- √表示支持，×表示该场景量化会异常。
- 权重ARQ中channel\_wise=true：表示每个channel独立量化，量化因子不同。
- 权重ARQ中asymmetric
  - true：表示权重量化使用非对称量化
  - false：表示权重量化使用对称量化。
  - true和false表示权重量化支持对称量化和非对称量化。
<!-- npu="910b" id5 -->
- FFN算子量化仅Atlas A2 训练系列产品/Atlas A2 推理系列产品支持。
<!-- end id5 -->

## 量化简易配置文件

如果要自动控制量化过程，例如控制哪些层是否量化、控制使用什么量化算法，则可以通过本章节构造的cfg配置文件实现。

### 配置文件参数

配置文件支持的消息以及对应参数说明如下：

- AMCTConfig：AMCT训练后量化的简易配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |optional|bool|activation_offset|数据量化是否带offset。全局配置参数。带offset：（默认值）数据量化使用非对称量化。不带offset：数据量化使用对称量化。|
    |optional|bool|joint_quant|是否进行Eltwise联合量化，默认为false，表示关闭联合量化功能。开启后对部分网络可能会存在性能提升但是精度下降的问题。|
    |repeated|string|skip_layers|不需要量化层的层名。|
    |repeated|string|skip_layer_types|不需要量化的层类型。|
    |optional|int32|version|简易配置文件的版本。|
    |optional|CalibrationConfig|common_config|通用的量化配置，全局量化配置参数。若某层未被override_layer_types或者override_layer_configs重写，则使用该配置。参数优先级：override_layer_configs>override_layer_types>common_config|
    |repeated|OverrideLayerType|override_layer_types|重写某一类型层的量化配置，即对哪些层进行差异化量化。例如全局量化配置参数配置的量化因子搜索步长为0.01，可以通过该参数对部分层进行差异化量化，可以配置搜索步长为0.02。参数优先级：override_layer_configs>override_layer_types>common_config|
    |repeated|OverrideLayer|override_layer_configs|重写某一层的量化配置，即对哪些层进行差异化量化。例如全局量化配置参数配置的量化因子搜索步长为0.01，可以通过该参数对部分层进行差异化量化，可以配置搜索步长为0.02。参数优先级：override_layer_configs>override_layer_types>common_config|
    |optional|bool|do_fusion|是否开启BN融合功能，默认为true，表示开启该功能。|
    |repeated|string|skip_fusion_layers|跳过BN融合的层，配置之后这些层不会进行BN融合。|
    |repeated|TensorQuantize|tensor_quantize|对网络模型中指定节点的输入Tensor进行训练后量化，来提高数据搬运时的推理性能。**当前仅支持对MaxPool/Add/算子做tensor量化。**|

- OverrideLayerType：支持量化的层类型的名称。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |required|string|layer_type|支持量化的层类型的名称。|
    |required|CalibrationConfig|calibration_config|重置的量化配置。|

- OverrideLayer：重置某层量化配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |required|string|layer_name|被重置层的层名。|
    |required|CalibrationConfig|calibration_config|重置的量化配置。|

- CalibrationConfig：Calibration量化的配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |-|ARQuantize|arq_quantize|权重量化算法配置。arq_quantize：ARQ量化算法配置。|
    |-|FMRQuantize|ifmr_quantize|数据量化算法配置。ifmr_quantize：IFMR量化算法配置。|
    |optional|bool|weight_compress_only|是否只进行权重量化。仅权重量化场景，支持的数据类型必须为Float32，Float16。true：只进行权重量化。false：权重和数据都量化。默认为false。|

- ARQuantize：ARQ权重量化算法配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |optional|bool|channel_wise|是否对每个channel采用不同的量化因子。true：每个channel独立量化，量化因子不同。false：所有channel同时量化，共享量化因子。|
    |optional|bool|asymmetric|是否对权重进行非对称量化。用于控制逐层量化算法的选择。**只在weight_compress_only为true时生效，若weight_compress_only设置为false，则asymmetric只能设置为false。**true：权重量化使用非对称量化（offset不为0）。false：权重量化使用对称量化（offset为0），默认为false。如果override_layer_configs、override_layer_types、common_config配置项都配置该参数，则生效优先级为：override_layer_configs>override_layer_types>common_config|
    |optional|uint32|quant_bits|权重量化位宽。支持配置为INT6、INT7、INT8，默认为INT8量化。该字段配置为INT6、INT7仅支持Conv2d类型算子。如果在common_config中配置quant_bits为INT6、INT7，则只对Conv2d算子生效，其他算子改为默认INT8。针对ONNX网络模型，如果在override_layer_types中指定Conv类算子quant_bits为INT6、INT7，则只对weight dim为4场景生效。|

- FMRQuantize：FMR数据量化算法配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |optional|float|search_range_start|量化因子搜索范围左边界。|
    |optional|float|search_range_end|量化因子搜索范围右边界。|
    |optional|float|search_step|量化因子搜索步长。|
    |optional|float|max_percentile|最大值搜索位置。|
    |optional|float|min_percentile|最小值搜索位置。|
    |optional|bool|asymmetric|是否对数据进行非对称量化。用于控制逐层量化算法的选择。true：非对称量化false：对称量化如果override_layer_configs、override_layer_types、common_config配置项都配置该参数，或者配置了activation_offset参数，则生效优先级为：override_layer_configs>override_layer_types>common_config>activation_offset|
    |optional|CalibrationDataType|dst_type|量化位宽，数据量化是采用INT8量化还是INT16量化，默认为INT8量化。**当前版本仅支持INT8量化。**|

- TensorQuantize：需要进行训练后量化的输入Tensor配置。

    |是否必填|类型|字段|说明|
    |--|--|--|--|
    |required|string|layer_name|需要对节点输入Tensor进行训练后量化的节点名称，当前仅支持对MaxPool算子的输入Tensor进行量化。|
    |required|uint32|input_index|需要对节点输入Tensor进行训练后量化的节点的输入索引。|
    |-|FMRQuantize|ifmr_quantize|数据量化算法配置。ifmr_quantize：IFMR量化算法配置。默认为IFMR量化算法。|

### 配置样例

- 基于该文件构造的**均匀量化简易配置文件**_quant_.cfg样例如下所示：_Optype_需要配置为基于Ascend IR定义的算子类型，详细对应关系请参见[支持量化的层及约束](#支持量化的层及约束)。

    ```yaml
    # global quantize parameter
    activation_offset : true
    joint_quant : false
    enable_auto_nuq : false
    version : 1
    skip_layers : "Optype"
    skip_layer_types: "Optype"
    do_fusion: true
    skip_fusion_layers : "Optype"
    common_config : {
        arq_quantize : {
            channel_wise : true
            quant_bits : 7

        }
        ifmr_quantize : {
            search_range_start : 0.7
            search_range_end : 1.3
            search_step : 0.01
            max_percentile : 0.999999
            min_percentile : 0.999999
            asymmetric : true
        }
    }

    override_layer_types : {
        layer_type : "Optype"
        calibration_config : {
            arq_quantize : {
                channel_wise : false
            }
            ifmr_quantize : {
                search_range_start : 0.8
                search_range_end : 1.2
                search_step : 0.02
                max_percentile : 0.999999
                min_percentile : 0.999999
                asymmetric : false
            }
        }
    }

    override_layer_configs : {
        layer_name : "Opname"
        calibration_config : {
            arq_quantize : {
                channel_wise : true
            }
            ifmr_quantize : {
                search_range_start : 0.8
                search_range_end : 1.2
                search_step : 0.02
                max_percentile : 0.999999
                min_percentile : 0.999999
                asymmetric : false
            }
        }
    }
    tensor_quantize {
        layer_name: "Opname"
        input_index: 0
        ifmr_quantize: {
            search_range_start : 0.7
            search_range_end : 1.3
            search_step : 0.01
            min_percentile : 0.999999
            asymmetric : false
           }
    }
    tensor_quantize {
        layer_name: "Opname"
        input_index: 0
    }
    ```

- 基于该文件构造的**仅权重量化简易配置文件**_quant_.cfg配置示例：

    ```yaml
    activation_offset : true
    joint_quant : false
    version : 1
    do_fusion: true
    common_config : {
       weight_compress_only : true
        arq_quantize : {
            channel_wise : true
            asymmetric : false

        }
    }

    override_layer_types : {
        layer_type : "Optype"
        calibration_config : {
            weight_compress_only : true
            arq_quantize : {
                channel_wise : true
                asymmetric : true
                quant_bits : 6
            }
        }
    }

    override_layer_configs : {
        layer_name : "Opname"
        calibration_config : {
            weight_compress_only : true
            arq_quantize : {
                channel_wise : true
                asymmetric : true
            }
        }
    }
    ```

## 支持的融合功能

量化过程中会对模型中的某些结构做算子融合，融合后图结构得到优化，从而提升网络推理性能，本节对使用到的融合功能进行详细介绍。

- Caffe框架：
  - Conv+BN+Scale融合，AMCT在量化前会对模型中的"Convolution+BatchNorm+Scale"结构做Conv+BN+Scale融合，融合后的"BatchNorm"、"Scale"层会被删除。
  - DeConv+BN+Scale融合，AMCT在量化前会对模型中的"Deconvolution+BatchNorm+Scale"结构做DeConv+BN+Scale融合，融合后的"BatchNorm"、"Scale"层会被删除。

- TensorFlow框架：
  - Conv+BN融合：AMCT在量化前会对模型中的"Conv2D/Conv3D+BatchNorm"结构做Conv+BN融合，融合后的"BatchNorm"层会被删除。

  - Depthwise\_Conv+BN融合：AMCT在量化前会对模型中的"DepthwiseConv2dNative+BatchNorm"结构做Depthwise\_Conv+BN融合，融合后的"BatchNorm"层会被删除。
  - Conv2DBackpropInput+BN融合：AMCT在量化前会对模型中的"Conv2DBackpropInput+BatchNorm"结构做Conv2DBackpropInput+BN融合，融合后的"BatchNorm"层会被删除。
  - Split+Conv+Concat融合：AMCT在量化前会对模型中的分组卷积Split+Conv（多个）+Bias（非必须）+Concat结构做group\_conv融合，减少校准流程的计算量并优化量化后模型的内存和性能。其中Split算子包含Split和SplitV，卷积算子包含Conv2d，Concat算子包含Concat和ConcatV2。

    要求：Split算子的split\_dim和Concat算子的concat\_dim与卷积的Cout轴index一致；各个卷积算子量化配置和算子属性要一致。

- ONNX框架：
  - Conv+BN融合，AMCT在量化前会对模型中的"Conv+BatchNormalization"结构做Conv+BN融合，融合后的"BatchNorm"层会被删除。
  - ConvTranspose+BN融合，AMCT在量化前会对模型中的"ConvTranspose+BatchNormalization"结构做ConvTranspose+BN融合，融合后的"BatchNorm"层会被删除。

- requant融合场景中的Relu6算子替换成Relu（Relu6无法做Requant融合，需替换为Relu）：由于Relu6算子在Relu的基础上增加了对6以上数值的截断，同时量化过程中也会对输入浮点数进行截断，故AscendDequant+Relu6+AscendQuant与AscendDequant+Relu+AscendQuant存在等价的场景可以进行替换。基于该背景AMCT对量化部署模型中的AscendDequant+Relu6+AscendQuant结构等价替换为AscendDequant+Relu+AscendQuant。

    该场景下需要满足限制条件才可以进行替换，条件为\(127-offset\)/scale<6，其中scale/offset为quant中取出的量化参数。

- BatchNorm+Mul+Add融合（适用于TensorFlow框架和ONNX框架）

    AMCT在量化前会先对模型中的"BatchNorm+Mul"结构做"BN+Mul"融合，融合后的"Mul"层会被删除；然后对模型中的"BatchNorm+Add"结构做"BN+Add"融合，融合后的"Add"层会被删除。

- BN小算子融合为BatchNorm大算子（适用于TensorFlow框架和ONNX框架）。

    AMCT对小算子结构的BN进行匹配，并将匹配到的小算子BN结构替换为大算子的BN结构。BN小算子结构融合的前提条件包括：

    1. BN结构中的data节点必须为可量化节点（Conv2D、DepthwiseConv2D、MatMulV2、Conv3D）。
    2. 如果结构中有除data、scale、offset、mean、variance和输出节点之外的节点与该结构外的节点连接，则不做融合。

    对于没有offset的BN结构，融合时会构造一个全0的offset节点；对于没有scale的BN结构，融合时会构造一个全1的scale节点。具体支持的小算子BN结构的场景如下所示：

  - 无offset，is\_training=False，data\_format=NHWC，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image1.png)

  - 无offset，is\_training=False，data\_format=NCHW，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image2.png)

  - 无scale和offset，is\_training=False，data\_format=NHWC，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image3.png)

  - 无scale和offset，is\_training=False，data\_format=NCHW，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image4.png)

  - 无scale，is\_training=False，data\_format=NHWC，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image5.png)

  - 无scale，is\_training=False，data\_format=NCHW，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image6.png)

  - is\_training=False，data\_format=NHWC，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image7.png)

  - is\_training=False，data\_format=NCHW，输入节点为Const类型，融合前后网络结构图为：

    ![fig](../figures/quant_image8.png)

  - 无offset，is\_training=False，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image9.png)

  - 无offset，is\_training=False，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image10.png)

  - 无scale和offset，is\_training=False，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image11.png)

  - 无scale和offset，is\_training=False，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image12.png)

  - 无scale，is\_training=False，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image13.png)

  - 无scale，is\_training=False，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image14.png)

  - is\_training=False，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image15.png)

  - is\_training=False，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image16.png)

  - 一种简化后的BN结构，无offset，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image17.png)

  - 一种简化后的BN结构，无offset，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image18.png)

  - 一种简化后的BN结构，无scale和offset，data\_format=NHWC，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image19.png)

  - 一种简化后的BN结构，无scale和offset，data\_format=NCHW，输入节点为Variable类型，融合前后网络结构图为：

    ![fig](../figures/quant_image20.png)

- bn\_branch类小算子融合成BN大算子

    AMCT对bn\_branch小算子结构的双BN结构进行匹配，并将匹配到的小算子结构替换为大算子的BN结构。如果结构中存在以下三种场景：两个BN分支的输入数据来源不同、结构输出为训练BN输出、推理BN的is\_training参数为True，则不做融合。算子融合过程会保留原结构中的推理BN大算子，并删除多余的Switch等小算子结构的节点，并重新添加保留BN算子的输入输出边，实现小算子融合成大算子。具体支持的小算子BN结构的场景如下所示：

    ![fig](../figures/quant_config_1.png)

    ![fig](../figures/quant_config_2.png)

    ![fig](../figures/quant_config_3.png)
