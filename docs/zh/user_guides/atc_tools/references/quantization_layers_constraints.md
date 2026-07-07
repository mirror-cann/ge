# 支持量化的层及约束

本章节给出不同框架可量化的层以及相关约束。

总体说明:

- 若网络模型输入数据类型或权重数据类型为Float16或混合精度类型（Float32/Float16共存），会关闭如下算子的量化功能：
 AvgPool、Pooling、AvgPoolV2、MaxPool、MaxPoolV3、Add、Eltwise、BatchMatMulV2（两路输入都为变量tensor）。
<!-- npu="IPV350" id1 -->
- INT16数据量化过程中，发现整网精度下降，可以通过精度比对工具，逐层比对原始模型和量化后模型输出误差（例如以余弦相似度作为标准，需要相似度达到0.99以上），找到误差较大的层，然后通过简易配置文件中的**dst\_type**参数将该层修改为INT8量化，重新进行量化。
<!-- end id1 -->
<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/references/quantization_layers_constraints_res.md#id1 -->

<!-- npu="A3,910b,IPV350" id17 -->
- **Caffe框架在如下产品形态已不演进，不保证功能可用：**

    <!-- npu="A3" id3 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品
    <!-- end id3 -->

    <!-- npu="910b" id2 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品
    <!-- end id2 -->

    <!-- npu="IPV350" id16 -->
    IPV350
    <!-- end id16 -->
<!-- end id17 -->

## 全量化

### 均匀量化支持的层及约束（Caffe框架）

| 支持的层类型 | 约束 | 对应Ascend IR定义的层类型 |
| --- | --- | --- |
| InnerProduct：全连接层 | transpose属性为false，axis为1 | FullyConnection |
| Convolution：卷积层 | filter维度为4 | Conv2D |
| Deconvolution：反卷积层 | dilation为1、filter维度为4 | Deconvolution |
| Pooling | - mode为1，全量化（weight+tensor），global_pooling为false，不支持移位N操作<br>  - mode为0，只做tensor量化 | Pooling |
| Eltwise | 只做tensor量化且operation=1 | Eltwise |

### 均匀量化支持的层及约束（TensorFlow框架）

<!-- npu="IPV350" id18 -->
如下表格不适用于IPV350，其约束请参见[IPV350支持的层及约束](#均匀量化支持的层及约束ipv350)。
<!-- end id18 -->

| 支持的层类型 | 约束 | 对应Ascend IR定义的层类型 |
| --- | --- | --- |
| MatMul：全连接层 | - transpose_a为False，transpose_b为False，adjoint_a为False，adjoint_b为False<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | MatMulV2 |
| Conv2D：卷积层 | weight的输入来源不含有placeholder等可动态变化的节点 | Conv2D |
| DepthwiseConv2dNative：Depthwise卷积层 | weight的输入来源不含有placeholder等可动态变化的节点 | DepthwiseConv2D |
| Conv2DBackpropInput | dilation为1，weight的输入来源不含有placeholder等可动态变化的节点 | Conv2DBackpropInput |
| BatchMatMulV2 | - adj_x=False<br>  - 如果第二路输入为常量const，则要求为2维<br>  - 当两路输入都为变量tensor时，仅支持INT8对称量化 | BatchMatMulV2 |
| AvgPool | 不支持移位N操作 | AvgPool |
| Conv3D | dilation_d为1 | Conv3D |
| MaxPool | 只做tensor量化 | MaxPool、MaxPoolV3 |
| Add | 只做tensor量化，且只支持单路输入的量化 | Add |

<!-- npu="A3,910b,310b" id5 -->
针对BatchMatMulV2量化层，两路输入都为变量tensor量化场景，只在以下产品能获得收益，其他产品量化后精度会下降：

<!-- npu="A3" id6 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id6 -->

<!-- npu="910b" id7 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id7 -->

<!-- npu="310b" id8 -->
Atlas 200I/500 A2 推理产品
<!-- end id8 -->
<!-- end id5 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/references/quantization_layers_constraints_res.md#id3 -->

### 均匀量化支持的层及约束（ONNX框架）

<!-- npu="IPV350" id19 -->
如下表格不适用于IPV350，其约束请参见[IPV350支持的层及约束](#均匀量化支持的层及约束ipv350)。
<!-- end id19 -->

| 支持的层类型 | 约束 | 对应Ascend IR定义的层类型 |
| --- | --- | --- |
| Conv：卷积层 | - filter维度为5的情况下，要求dilation_d为1<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | Conv2D、Conv3D |
| Gemm：广义矩阵乘 | - transpose_a=false<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | MatMulV2 |
| ConvTranspose：转置卷积 | - dilation为1、filter维度为4<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | Conv2DTranspose |
| MatMul | - 如果第二路输入为常量const，则要求为2维<br>  - 当两路输入都为变量tensor时，仅支持INT8对称量化<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | BatchMatMulV2 |
| AveragePool | global_pooling为false，不支持移位N操作 | AvgPoolV2 |
| MaxPool | 只做tensor量化 | MaxPool、MaxPoolV3 |
| Add | 只做tensor量化，且只支持单路输入的量化 | Add |

<!-- npu="A3,910b,310b" id23 -->
针对MatMul量化层，两路输入都为变量tensor量化场景，只在以下产品能获得收益，其他产品量化后精度会下降：

<!-- npu="A3" id24 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品
<!-- end id24 -->

<!-- npu="910b" id25 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id25 -->

<!-- npu="310b" id26 -->
Atlas 200I/500 A2 推理产品
<!-- end id26 -->
<!-- end id23 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/references/quantization_layers_constraints_res.md#id4 -->

<!-- npu="IPV350" id20 -->
### 均匀量化支持的层及约束（IPV350）

| 框架 | 支持的层类型 | 约束 | 对应Ascend IR定义的层类型 |
| --- | --- | --- | --- |
| TensorFlow | MatMul：全连接层 | - transpose_a为False, transpose_b为False，adjoint_a为False，adjoint_b为False<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | MatMulV2 |
|TensorFlow| Conv2D：卷积层 | weight的输入来源不含有placeholder等可动态变化的节点 | Conv2D |
|TensorFlow |Conv2DBackpropInput | dilation为1，weight的输入来源不含有placeholder等可动态变化的节点 |Conv2DBackpropInput |
|TensorFlow |DepthwiseConv2dNative | weight的输入来源不含有placeholder等可动态变化的节点 | DepthwiseConv2D |
| ONNX | ConvTranspose | - dilation为1、filter维度为4<br>  - weight的输入来源不含有placeholder等可动态变化的节点 | Conv2DTranspose |
<!-- end id20 -->

## 仅权重量化

<!-- npu="IPV350" id21 -->
**该版本不支持仅权重量化特性。**
<!-- end id21 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/references/quantization_layers_constraints_res.md#id2 -->

<!-- npu="910b,310p" id22 -->
**仅权重量化特性，仅支持以下产品类型：**
<!-- npu="310p" id13 -->
Atlas 推理系列产品
<!-- end id13 -->
<!-- npu="910b" id14 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品
<!-- end id14 -->
<!-- end id22 -->

**表 2**  仅权重量化场景支持的层及约束

|**Ascend IR定义的层**类型|仅权重量化权重ARQ中channel_wise=true|仅权重量化权重ARQ中asymmetric=true或false|权重和数据都量化权重ARQ中channel_wise=true|权重和数据都量化权重ARQ中asymmetric=true|约束|
|--|--|--|--|--|--|
|MatMulV2|√|true|×|×|第二路的输入来源不含有placeholder等可动态变化的节点。|
|BatchMatMulV2|√|true|×|×|第二路的输入来源不含有placeholder等可动态变化的节点。|
|FFN|√|true和false|×|×|- FFN算子的expert_tokens输入不为空。<br>- FFN算子的两个权重为Float16的常量。<br>- FFN算子的antiquant_scale1、antiquant_scale2、antiquant_offset1、antiquant_offset2四个输入为空。<br>- 权重不支持权重共享。

其中：

- √表示支持，×表示该场景量化会异常。
- 权重ARQ中channel\_wise=true：表示每个channel独立量化，量化因子不同。
- 权重ARQ中asymmetric
  - true：表示权重量化使用非对称量化
  - false：表示权重量化使用对称量化。
  - true和false表示权重量化支持对称量化和非对称量化。
<!-- npu="910b" id4 -->
- FFN算子量化仅Atlas A2 训练系列产品/Atlas A2 推理系列产品支持。
<!-- end id4 -->
