# 什么是AIPP

本节介绍什么是AIPP，AIPP分类以及包括的特性。

<!-- npu="IPV350" id1 -->
**IPV350不支持AIPP特性。**
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/AIPP/aipp_intro_res.md#id1 -->

AIPP（Artificial Intelligence Pre-Processing）人工智能预处理，用于在AI Core上完成数据预处理，包括改变图像尺寸、色域转换（转换图像格式）、减均值/乘系数（改变图像像素），数据预处理之后再进行真正的模型推理。

该模块功能与DVPP（Digital Vision Pre-Processing）相似，都可以实现媒体数据处理的功能，两者的功能范围有重合，比如改变图像尺寸、转换图像格式，但功能范围也有不同的，比如DVPP可以做图像编解码、视频编解码，AIPP可以做归一化配置。与DVPP不同的是，AIPP主要用于在AI Core上完成数据预处理，DVPP是AI处理器内置的图像处理单元，通过媒体数据处理接口提供强大的媒体处理硬加速能力。

AIPP、DVPP可以分开独立使用，也可以组合使用。组合使用场景下，一般先使用DVPP对图片/视频进行解码、抠图、缩放等基本处理，但由于DVPP硬件上的约束，DVPP处理后的图片格式、分辨率有可能不满足模型的要求，因此还需要再经过AIPP进一步做色域转换、抠图、填充等处理。

<!-- npu="310p" id2 -->
Atlas 推理系列产品场景：DVPP模块输出的图片多为对齐后的YUV420SP类型，不支持输出RGB图片；如果模型需要RGB格式的图片，业务流需要使用AIPP模块转换对齐后YUV420SP类型图片的格式，并抠出模型需要的输入图片。
<!-- end id2 -->

AIPP根据配置方式不同，分为静态AIPP和动态AIPP；如果要将原始图片输出为满足推理要求的图片格式，则需要使用色域转换功能；如果要输出固定大小的图片，则需要使用AIPP提供的Crop（抠图）、Padding（补边）功能。

## 静态AIPP和动态AIPP

在开启AIPP功能时，您只能选择配置静态AIPP或动态AIPP方式来处理图片，不能同时配置静态AIPP和动态AIPP两种方式，开启AIPP通过**aipp\_mode**参数控制。具体配置示例请参见[AIPP配置示例](aipp_config_example.md)，关于参数解释请参见[配置文件模板](config_file_template.md)。

- 静态AIPP：模型转换时设置AIPP模式为静态，同时设置AIPP参数，模型生成后，AIPP参数值被保存在离线模型中，每次模型推理过程采用固定的AIPP预处理参数进行处理，而且在之后的推理过程中无法通过业务代码进行直接的修改。

    如果使用静态AIPP方式，多batch情况下共用同一份AIPP参数。

- 动态AIPP：模型转换时设置AIPP模式为动态，每次在执行推理前，根据需求动态修改AIPP参数值，然后在模型执行时可使用不同的AIPP参数。动态AIPP参数值会根据需求在不同的业务场景下选用合适的参数（如不同摄像头采用不同的归一化参数，输入图片格式需要兼容YUV420和RGB等）。

    如果模型转换时设置了动态AIPP，则使用应用工程进行模型推理时，需要在acl提供的**aclmdlExecute**接口之前，调用**aclmdlSetInputAIPP**接口，设置模型推理的动态AIPP数据。接口详细说明请参见[模型执行](../../../api/graph_engine_api/c/acl/model_execute_APIs.md)。

    如果使用动态AIPP方式，多batch使用不同的参数，体现在动态参数结构体中，每个batch可以配置不同的crop等参数。关于动态参数结构体，请参见[动态AIPP的参数输入结构](aipp_config_example.md#动态aipp的参数输入结构)。

AIPP支持的图像输入格式包括：YUV420SP\_U8、RGB888\_U8、XRGB8888\_U8、YUV400\_U8。

## 色域转换

色域转换，用于将输入的图片格式，转换为模型需要的图片格式，在开启AIPP功能时，通过**csc\_switch**参数控制色域转换功能是否开启，参数解释请参见[配置文件模板](config_file_template.md)。

一旦确认了AIPP处理前与AIPP处理后的图片格式，即可确定色域转换其他相关的参数值，本文提供相关模板可以供用户使用，无需再次修改，配置示例请参见[色域转换配置说明](CSC_config.md)。

## 改变图像尺寸

AIPP功能中的改变图像尺寸操作由Crop（抠图）、Padding（补边）完成，分别对应配置模板中的crop、padding参数。参数解释请参见[配置文件模板](config_file_template.md)。

关于该功能的详细说明以及AIPP参数配置示例请参见[Crop/Padding配置说明](crop_padding_config.md)。

## AIPP对模型输入大小的校验说明

如果有配置AIPP，无论静态AIPP还是动态AIPP，最终生成离线模型的输入大小（即input\_size）均会被Crop、Padding等操作影响。本节给出对模型输入大小的约束说明。

假设模型的Batch数量为N（如果为动态batch场景，N为最大档位数的取值），模型输入图片的宽为src\_image\_size\_w，高为src\_image\_size\_h，最后模型输入的Size的计算公式如下所示。

### 静态AIPP对模型输入大小的校验

- **YUV400\_U8**

    ```text
    N * src_image_size_w * src_image_size_h * 1
    ```

- **YUV420SP\_U8**

    ```text
    N * src_image_size_w * src_image_size_h * 1.5
    ```

- **XRGB8888\_U8**

    ```text
    N * src_image_size_w * src_image_size_h * 4
    ```

- **RGB888\_U8**

    ```text
    N * src_image_size_w * src_image_size_h * 3
    ```

### 动态AIPP对模型输入大小的校验

如果为动态AIPP，模型转换时，ATC会为动态AIPP新增一个模型输入，用于接收模型推理阶段通过调用**aclmdlSetInputAIPP**接口后传入的AIPP参数，该场景下新增输入节点大小计算公式如下，接口详细说明请参见[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)。

```text
sizeof(kAippDynamicPara) - sizeof(kAippDynamicBatchPara) + batch_count * sizeof(kAippDynamicBatchPara)
```

kAippDynamicPara以及kAippDynamicBatchPara参数解释请参见[动态AIPP的参数输入结构](aipp_config_example.md#动态aipp的参数输入结构)。
