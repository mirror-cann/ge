# 量化

## 简介

量化是指对模型的权重（weight）和数据（activation）进行低比特处理，让最终生成的网络模型更轻量化，从而达到节省网络模型存储空间、降低传输时延、提高计算效率，达到性能提升与优化的目标。本节介绍如何对Graph进行量化。量化运行原理如下图所示。

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quantization_res.md#id1 -->

**图 1**  量化原理
![图示](../figures/quantization_principle.png "量化原理")

量化过程中会实现模型部署优化（主要为算子融合）功能，如下图所示。

**图 2**  算子融合原理
![图示](../figures/op_fusion_principle.png "算子融合原理")

量化又分为自动量化和手工量化：

- 自动量化：通过aclgrphCalibration接口来自动插入量化算子，量化过程中会对模型中的某些结构做算子融合。推荐使用自动量化。
- 手工量化：手动修改模型，插入量化算子。

## 自动量化

本节介绍如何通过aclgrphCalibration接口对Graph进行自动量化。

### 功能介绍

通过调用[aclgrphCalibration](../../../api/graph_engine_api/cpp/ge/aclgrphCalibration.md)接口对非量化的Graph进行自动量化。量化过程中会对模型中的某些结构做算子融合，融合后Graph结构得到优化，从而提升网络推理性能。具体支持的融合功能请参见[支持的融合功能](../appendix/quantization_config.md#支持的融合功能)。支持量化的层及约束请参考[支持量化的层及约束](../appendix/quantization_config.md)。

使用该特性时，需要单独安装AMCT（acl）软件包，该包的获取以及安装方法请参见《AMCT模型压缩工具》中的“准备环境 \> 获取软件包、上传软件包、安装工具”章节。

### 调用示例

本章节以TensorFlow框架网络模型量化为例进行说明。

1. 添加头文件。

    ```c++
    #include "amct/acl_graph_calibration.h"
    #include "amct/acl_calibration_configs.h"
    #include "amct/amct_error_code.h"
    ```

2. 通过解析或手动构造方式创建Graph对象。

    ```c++
    #include "parser/tensorflow_parser.h"

    ge::Graph graph("origin");
    std::map<ge::AscendString, ge::AscendString> parseOptions;
    auto ret = ge::aclgrphParseTensorFlow("./path/tf_test.pb", parseOptions, graph);
    ```

3. 定义aclgrphCalibration接口所需配置参数。

    ```c++
    std::map<ge::AscendString, ge::AscendString> quantizeConfigs = {};
    quantizeConfigs[ge::AscendString(amct::aclCaliConfigs::INPUT_DATA_DIR)] = ge::AscendString("./path/cali_data.bin");
    quantizeConfigs[ge::AscendString(amct::aclCaliConfigs::INPUT_SHAPE)] = ge::AscendString("input:16,224,224,3");
    quantizeConfigs[ge::AscendString(amct::aclCaliConfigs::SOC_VERSION)] = ge::AscendString("SOC_VERSION"); // SOC_VERSION请配置为实际的AI处理器型号
    ```

    如果需要自行控制量化过程，可以使用CONFIG\_FILE参数项传入控制量化过程的简易配置文件。配置文件的示例请参考[量化简易配置文件](../appendix/quantization_config.md#量化简易配置文件)。

    CONFIG\_FILE参数的配置示例如下：

    ```c++
    quantizeConfigs[ge::AscendString(amct::aclCaliConfigs::CONFIG_FILE)]=ge::AscendString("./calibration.cfg");
    ```

4. 调用接口执行量化改图。

    ```c++
    ret = ge::aclgrphCalibration(graph, quantizeConfigs);
    if (ret != ge::GRAPH_SUCCESS) {
        return FAILED;
    }
    return SUCCESS;
    ```

> [!NOTE]说明
>
>编译上述程序的脚本时需要链接libamctacl.so，并添加链接时动态库的搜索路径$\{ASCEND\_PATH\}/compiler/lib64，其中ASCEND\_PATH为CANN软件包安装路径，以root用户默认安装路径为例，/usr/local/Ascend/cann。

## 手工量化

本节介绍如何手动修改模型插入量化算子，实现量化功能。

### 功能介绍

用户可通过自己的框架和工具完成量化，并将这些量化参数（scale<sub>d、</sub>scale<sub>w</sub>、offset<sub>d</sub>）在模型构建时注入到模型中。

> [!NOTE]说明
>
>- 当前仅Conv2D/DepthwiseConv2D/FullyConnection算子支持量化。
>- 网络中Conv2D/DepthwiseConv2D/FullyConnection算子输入数据的Channel维度小于等于16时，由于Padding补齐，INT8量化无性能收益。因此量化要求Conv2D/DepthwiseConv2D/FullyConnection算子输入数据的Channel维度大于16，否则获取不到性能收益。

以Conv2D算子进行INT8量化为例，通过在Conv2D算子前插入AscendQuant量化算子，在Conv2D算子后插入AscendDequant反量化算子实现模型量化，如[图1](#fig1)所示。

AscendQuant量化算子的作用是将float类型的数据转换为int8类型，即：data<sub>int8</sub>=round\[\(data<sub>float</sub>\*scale\)+offset\]，其中scale=1/scale<sub>d，</sub>offset=offset<sub>d</sub>。此处的round算法类似于C语言rint取整模式中的FE\_TONEAREST模式。

AscendDequant反量化算子的作用是将int32类型的数据转换为float32类型，即：data<sub>float</sub><sub>32</sub>=data<sub>int32</sub>\*deq\_scale，其中deq\_scale=scale<sub>d</sub>\*scale<sub>w</sub><sub>。</sub>

**图 1**  量化示意图<a id="fig1"></a>
![图示](../figures/quantization_diagram.png "量化示意图")

### 在Conv2D算子前插入AscendQuant算子

AscendQuant算子原型定义：

```c++
REG_OP(AscendQuant)
    .INPUT(x, TensorType({DT_FLOAT16, DT_FLOAT32}))
    .OUTPUT(y, TensorType({DT_INT8, DT_INT4}))
    .REQUIRED_ATTR(scale, Float)
    .REQUIRED_ATTR(offset, Float)
    .ATTR(sqrt_mode, Bool, false)
    .ATTR(round_mode, String, "Round")
    .ATTR(dst_type, Int, DT_INT8)
    .OP_END_FACTORY_REG(AscendQuant)
```

可以看到AscendQuant算子有1个输入x，两个必选属性scale和offset，三个可选属性sqrt\_mode、round\_mode、dst\_type，参数含义如下：

- x：指定AscendQuant算子输入，Tensor类型，支持的数据类型为float16和float32。
- y：算子输出，Tensor类型，支持的数据类型为int8和int4。
- scale：指定量化系数scale=1/scale<sub>d</sub>，支持float32和float16数据类型，建议在float16数据类型表达范围内，如果超出了float16表达范围，请配置sqrt\_mode为True。
- offset：指定量化偏移量offset=1/offset<sub>d</sub>，支持float32和float16数据类型。
- sqrt\_mode：是否对scale进行开平方处理，取值为False和True，建议保持默认False。如果scale超出了float16表达范围，为了不损失精度，请配置sqrt\_mode为True，系统会对scale做开平方处理。
- round\_mode：float类型到int类型的转换方式，默认为Round，支持配置为：Round、Floor、Ceiling、Truncate。
- dst\_type：指定输出数据类型，默认为2，相当于数据类型为DT\_INT8。

根据AscendQuant算子原型定义创建AscendQuant算子实例：

```c++
auto quant = op::AscendQuant("quant")
  .set_input_x(data)
  .set_attr_scale(1.00049043)      // 指定量化系数
  .set_attr_offset(-128.0);        // 指定偏移量
```

### Conv2D算子

Conv2D算子的输入为AscendQuant，并设置输出type为int32。

```c++
// const op: conv2d weight
auto weight_shape = ge::Shape({ 5,17,1,1 });                     // 设定权重张量形状
TensorDesc desc_weight_1(weight_shape, FORMAT_NCHW, DT_INT8);    // 权重张量描述，数据类型INT8
Tensor weight_tensor(desc_weight_1);                             // 创建权重张量
uint32_t weight_1_len = weight_shape.GetShapeSize();             // 计算权重张量的元素个数
bool res = GetConstTensorFromBin(PATH+"const_0.bin", weight_tensor, weight_1_len);  // 从文件读取权重数据
if(!res) {
    std::cout << "GetConstTensorFromBin Failed!" << std::endl;
    return -1;
}
auto conv_weight = op::Const("const_0")      // 创建Const节点（权重）
    .set_attr_value(weight_tensor);          // 给Const节点设置权重张量

// const op: conv2d bias
auto bias_shape = ge::Shape({ 5 });          // 设定 bias 张量形状
TensorDesc desc_bias(bias_shape, FORMAT_NCHW, DT_INT32);                   // bias张量描述，数据类型INT32
Tensor bias_tensor(desc_bias);               // 创建 bias 张量
uint32_t bias_len = bias_shape.GetShapeSize() * sizeof(int32_t);           // 计算bias张量的字节大小
res = GetConstTensorFromBin(PATH + "const_1.bin", bias_tensor, bias_len);  // 从文件读取bias数据
if(!res) {
    std::cout << "GetConstTensorFromBin Failed!" << std::endl;
    return -1;
}
auto conv_bias = op::Const("const_1")        // 创建Const节点（bias）
    .set_attr_value(bias_tensor);

// conv2d op
auto conv2d = op::Conv2D("Conv2d")            // 创建Conv2D节点
    .set_input_x(quant)                      // AscendQuant作为Conv2D算子的输入
    .set_input_filter(conv_weight)            // 设定卷积核权重
    .set_input_bias(conv_bias)                // 设定卷积bias
    .set_attr_strides({ 1, 1, 1, 1 })         // stride
    .set_attr_pads({ 0, 0, 0, 0 })            // padding
    .set_attr_dilations({ 1, 1, 1, 1 });      // dilation

// 为Conv2D节点的输入输出张量设置描述
TensorDesc conv2d_input_desc_x(ge::Shape(), FORMAT_NCHW, DT_INT8);        // 量化后，输入x的type为INT8
TensorDesc conv2d_input_desc_filter(ge::Shape(), FORMAT_NCHW, DT_INT8);
TensorDesc conv2d_input_desc_bias(ge::Shape(), FORMAT_NCHW, DT_INT32);
TensorDesc conv2d_output_desc_y(ge::Shape(), FORMAT_NCHW, DT_INT32);
// 更新描述
conv2d.update_input_desc_x(conv2d_input_desc_x);
conv2d.update_input_desc_filter(conv2d_input_desc_filter);
conv2d.update_input_desc_bias(conv2d_input_desc_bias);
conv2d.update_output_desc_y(conv2d_output_desc_y);
```

GetConstTensorFromBin函数实现请参见[从文件读入权重数据](../construct_graph/op_expr_samples_in_graph.md#定义常量节点const)。

### 在Conv2D算子后插入AscendDequant算子

AscendDequant算子原型定义：

```c++
REG_OP(AscendDequant)
    .INPUT(x, TensorType({DT_INT32}))
    .INPUT(deq_scale, TensorType({DT_FLOAT16, DT_UINT64}))
    .OUTPUT(y, TensorType({DT_FLOAT16, DT_FLOAT}))
    .ATTR(sqrt_mode, Bool, false)
    .ATTR(relu_flag, Bool, false)
    .ATTR(dtype, Int, DT_FLOAT)
    .OP_END_FACTORY_REG(AscendDequant)
```

可以看到AscendDequant算子有2个输入x和deq\_scale，三个可选属性sqrt\_mode、relu\_flag、dtype，参数含义如下：

- x：指定AscendDequant算子输入，Tensor类型，支持的数据类型为int32。
- deq\_scale：指定反量化系数deq\_scale=scale<sub>d</sub>\*scale<sub>w，</sub>Tensor类型，支持的数据类型为uint64。deq\_scale的shape可以为1，或者和Conv2D输出数据的Channel维度保持一致。

    > [!NOTE]说明
    >
    >要求用户把scale<sub>d</sub>和scale<sub>w</sub>相乘得到的float32数据类型转换为uint64类型，并填入到deq\_scale的低32位中，高32位要求全部为0。

    ```c++
    import numpy as np
    def trans_float32_scale_deq_to_uint64(scale_deq):
        float32_scale_deq = np.array(scale_deq, np.float32)
        uint32_scale_deq = np.frombuffer(float32_scale_deq, np.uint32)
        uint64_result = np.zeros(float32_scale_deq.shape, np.uint64)
        uint64_result |= np.uint64(uint32_scale_deq)
        return uint64_result
    ```

- sqrt\_mode：是否对deq\_scale进行开平方处理，取值为False和True，建议保持默认False。如果deq\_scale超出了float16表达范围，为了不损失精度，请配置sqrt\_mode为True，同时将deq\_scale开平方后填入。
- relu\_flag：是否执行Relu，取值为False和True，默认值为False。
- dtype：指定输出数据类型，默认为0，相当于数据类型为DT\_FLOAT。

根据AscendDequant算子原型定义创建AscendDequant算子实例：

```c++
// 构造dequant_scale
TensorDesc desc_dequant_shape(ge::Shape({ 5 }), FORMAT_NCHW, DT_UINT64);
Tensor dequant_tensor(desc_dequant_shape);
uint32_t dequant_scale_len = 5 * sizeof(uint64_t);
res = GetConstTensorFromBin(PATH + "const_2.bin", dequant_tensor, dequant_scale_len);
if(!res) {
    std::cout << "GetConstTensorFromBin Failed!" << std::endl;
    return -1;
}
auto dequant_scale = op::Const("dequant_scale")        // 创建Const节点（dequant_scale）
    .set_attr_value(dequant_tensor);

// 定义AscendDequant算子
auto dequant = op::AscendDequant("dequant")
  .set_input_x(conv2d)                                  // Conv2D作为AscendDequant算子的输入
  .set_input_deq_scale(dequant_scale);
```

GetConstTensorFromBin函数实现请参见[从文件读入权重数据](../construct_graph/op_expr_samples_in_graph.md#定义常量节点const)。

将AscendDequant的输出作为其他算子的输入，或者作为整个graph的输出。

```c++
auto bias_add_1 = op::BiasAdd("bias_add_1")
   .set_input_x(dequant)
   .set_input_bias(bias_weight_1)
   .set_attr_data_format("NCHW");
```
