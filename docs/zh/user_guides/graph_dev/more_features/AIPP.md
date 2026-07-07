# AIPP

本节介绍构图过程中如何开启AIPP特性。

<!-- npu="IPV350" id1 -->
IPV350不支持AIPP特性。
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/graph_dev/AIPP_res.md#id1 -->

## 功能介绍

AIPP（Artificial Intelligence Pre-Processing）人工智能预处理，用于在AI Core上完成图像预处理，包括色域转换（转换图像格式）、图像归一化（减均值/乘系数）和抠图（指定抠图起始点，抠出神经网络需要大小的图片）。AIPP区分为静态AIPP和动态AIPP，只能二选一，不能同时支持。

- 静态AIPP：模型编译时设置AIPP模式为静态，同时设置AIPP参数，模型生成后，AIPP参数值被保存在离线模型中，每次模型推理过程采用固定的AIPP预处理参数（无法修改）。
- 动态AIPP：模型编译时设置AIPP模式为动态，每次模型推理前，根据需求，在执行模型前设置动态AIPP参数值，然后在模型执行时可使用不同的AIPP参数。动态AIPP在根据业务要求改变预处理参数的场合下使用（如不同摄像头采用不同的归一化参数，输入图片格式需要兼容YUV420和RGB等）。

关于AIPP功能的详细介绍请参考[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。

## 使用方法

下面介绍如何在模型构建时支持AIPP功能：

1. Data算子输入数据的Channel维度的元素个数和图片实际保持一致。

    ```c++
    auto shape_data = vector<int64_t>({ 1,10,12,12 });
    // 创建TensorDesc
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_NCHW, DT_FLOAT16);
    // 创建Data算子（输入节点）
    auto data = op::Data("data");
    // 给Data算子的输入张量描述符赋值
    data.update_input_desc_x(desc_data);
    data.update_output_desc_y(desc_data);
    ```

2. 准备AIPP配置文件，配置说明请参考《ATC离线模型编译工具》，下面给出一些配置示例：

    静态AIPP配置文件示例：

    ```text
    aipp_op {
    aipp_mode:static          # 表示静态AIPP
    input_format:YUV420SP_U8
    csc_switch:true
    var_reci_chn_0:0.00392157
    var_reci_chn_1:0.00392157
    var_reci_chn_2:0.00392157
    }
    ```

    动态AIPP配置文件示例（除了以下字段外，其他无需配置）：

    ```text
    aipp_op {
    aipp_mode:dynamic         # 表示动态AIPP
    related_input_rank: 0     # 可选，标识对模型的第几个输入做AIPP处理，从0开始，默认为0
    max_src_image_size:752640 # 输入图像最大的size，动态AIPP必须配置
    }
    ```

3. 模型编译时，在aclgrphBuildModel接口options中设置**INSERT\_OP\_FILE**和**INPUT\_FORMAT**。

    ```c++
    void PrepareOptions(std::map<AscendString, AscendString>& options) {
        options.insert({
            {ge::ir_option::EXEC_DISABLE_REUSED_MEMORY, "1"},
           {ge::ir_option::INSERT_OP_FILE, PATH + "aipp_nv12_img.cfg"},
           {ge::ir_option::INPUT_FORMAT, "NCHW"}
            });
    }
    ```

4. 编译生成的离线模型自动插入aipp算子。

    ![](../figures/aipp_op.png)

## 使用注意事项

- 如果用户设置了**静态**AIPP功能，同时又通过INPUT\_SHAPE设置了动态shape范围参数，则：

    如果模型只有一个输入，该场景不支持；如果模型有多个输入，则必须对不同的输入节点进行设置，比如一个输入节点设置静态AIPP，另外一个节点设置动态shape。

- 如果用户设置了**静态AIPP**功能，同时又通过DYNAMIC\_BATCH\_SIZE设置了动态分辨率（输入图片的宽和高不确定）：

    该场景下，AIPP配置文件中不能开启Crop和Padding功能，并且需要将配置文件中的src\_image\_size\_w和src\_image\_size\_h取值设置为0。

- 如果用户设置了**动态**AIPP功能，同时又通过INPUT\_SHAPE设置了动态shape范围参数，则AIPP输出的宽和高要在INPUT\_SHAPE所设置的范围内。
- 如果用户设置了**动态AIPP**功能，同时又通过DYNAMIC\_BATCH\_SIZE设置了动态batch\_size：

    实际推理时，调用**aclmdlSetInputAIPP**接口，设置动态AIPP相关参数值时，需确保batch\_size要设置为最大Batch数。接口详细说明请参见《GE图引擎 API》中的“C语言接口 \> 模型管理和单算子调用接口 \>模型管理 \> 模型执行 \> aclmdlSetInputAIPP”。

- 如果用户设置了**动态AIPP**功能，同时又通过DYNAMIC\_IMAGE\_SIZE设置了动态分辨率（输入图片的宽和高不确定）：

    实际推理时，调用**aclmdlSetInputAIPP**接口，设置动态AIPP相关参数值时，不能开启Crop和Padding功能。该场景下，还需要确保通过aclmdlSetInputAIPP接口设置的宽和高与**aclmdlSetDynamicHWSize**接口设置的宽、高相等，都必须设置成动态分辨率最大档位的宽、高。接口详细说明请参见《GE图引擎 API》中的“C语言接口 \> 模型管理和单算子调用接口 \> 模型管理 \> 模型执行”章节。
