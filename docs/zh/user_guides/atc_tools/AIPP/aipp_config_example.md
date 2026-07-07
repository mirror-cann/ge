# AIPP配置示例

## 静态AIPP配置示例

AIPP配置文件支持定义多组AIPP配置，对不同的模型输入进行不同的AIPP处理，配置多组AIPP参数时，将一组AIPP配置放到一个aipp\_op配置项里；如果模型只有一个输入，则只需要配置第一组aipp\_op即可。

如下示例以网络模型为多输入时进行说明：

> [!NOTE]说明
>
>- 静态AIPP+动态shape场景：模型转换时，通过[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数设置了静态AIPP，又通过[--input\_shape](../CLI_options/--input_shape.md)设置了动态shape，则：
> 如果模型只有一个输入，该场景不支持；如果模型有多个输入，则必须对不同的输入节点进行设置，比如一个输入节点设置静态AIPP，另一个节点设置动态shape。
>- 如果模型转换时，用户设置了[--dynamic\_image\_size](../CLI_options/--dynamic_image_size.md)动态分辨率参数，即输入图片的宽和高不确定，同时又通过[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数设置了静态AIPP功能：该场景下，AIPP配置文件中不能开启Crop和Padding功能，并且需要将配置文件中的src\_image\_size\_w和src\_image\_size\_h取值设置为0。

- 使用related\_input\_rank参数标识，对模型第几个输入进行AIPP处理，如下配置定义了两组AIPP参数，分别对模型第一个和第二个输入进行AIPP处理：

    ```textproto
    aipp_op {
           aipp_mode : static
           related_input_rank : 0  # 标识对第1个输入进行AIPP处理
           src_image_size_w : 608
           src_image_size_h : 608
           crop : false
           input_format : YUV420SP_U8
           csc_switch : true
           rbuv_swap_switch : false
           matrix_r0c0 : 298
           matrix_r0c1 : 0
           matrix_r0c2 : 409
           matrix_r1c0 : 298
           matrix_r1c1 : -100
           matrix_r1c2 : -208
           matrix_r2c0 : 298
           matrix_r2c1 : 516
           matrix_r2c2 : 0
           input_bias_0 : 16
           input_bias_1 : 128
           input_bias_2 : 128
           mean_chn_0 : 104
           mean_chn_1 : 117
           mean_chn_2 : 123
    }
    aipp_op {
           aipp_mode : static
           related_input_rank : 1   # 标识对第2个输入进行AIPP处理
           src_image_size_w : 608
           src_image_size_h : 608
           crop : false
           input_format : YUV420SP_U8
           csc_switch : true
           rbuv_swap_switch : false
           matrix_r0c0 : 298
           matrix_r0c1 : 0
           matrix_r0c2 : 409
           matrix_r1c0 : 298
           matrix_r1c1 : -100
           matrix_r1c2 : -208
           matrix_r2c0 : 298
           matrix_r2c1 : 516
           matrix_r2c2 : 0
           input_bias_0 : 16
           input_bias_1 : 128
           input_bias_2 : 128
           mean_chn_0 : 104
           mean_chn_1 : 117
           mean_chn_2 : 123
    }
    ```

- 使用related\_input\_name参数标识，对模型第几个输入进行AIPP处理，此处需要填写为模型输入的name（input对应的值）或者模型首层节点的输出（top参数对应的取值），**该参数只适用于Caffe网络模型**，且不能与related\_input\_rank参数同时使用，如果同时配置，related\_input\_name优先级高于related\_input\_rank。

    如下配置定义了两组AIPP参数，分别对模型第一个和第二个输入进行AIPP处理：

    ```textproto
    aipp_op {
           aipp_mode : static
          related_input_name : "data"  # 标识对第1个输入进行AIPP处理
           src_image_size_w : 608
           src_image_size_h : 608
           crop : false
           input_format : YUV420SP_U8
           csc_switch : true
           rbuv_swap_switch : false
           matrix_r0c0 : 298
           matrix_r0c1 : 0
           matrix_r0c2 : 409
           matrix_r1c0 : 298
           matrix_r1c1 : -100
           matrix_r1c2 : -208
           matrix_r2c0 : 298
           matrix_r2c1 : 516
           matrix_r2c2 : 0
           input_bias_0 : 16
           input_bias_1 : 128
           input_bias_2 : 128
           mean_chn_0 : 104
           mean_chn_1 : 117
           mean_chn_2 : 123
    }
    aipp_op {
           aipp_mode : static
          related_input_name : "im_info"   # 标识对第2个输入进行AIPP处理
           src_image_size_w : 608
           src_image_size_h : 608
           crop : false
           input_format : YUV420SP_U8
           csc_switch : true
           rbuv_swap_switch : false
           matrix_r0c0 : 298
           matrix_r0c1 : 0
           matrix_r0c2 : 409
           matrix_r1c0 : 298
           matrix_r1c1 : -100
           matrix_r1c2 : -208
           matrix_r2c0 : 298
           matrix_r2c1 : 516
           matrix_r2c2 : 0
           input_bias_0 : 16
           input_bias_1 : 128
           input_bias_2 : 128
           mean_chn_0 : 104
           mean_chn_1 : 117
           mean_chn_2 : 123
    }
    ```

## 动态AIPP配置示例

AIPP配置文件支持定义多组AIPP配置，对不同的模型输入进行不同的AIPP处理，配置多组AIPP参数时，将一组AIPP配置放到一个aipp\_op配置项里；如果模型只有一个输入，则只需要配置第一组aipp\_op即可。

如下示例以网络模型为多输入时进行说明。

### 配置示例

> [!NOTE]说明
>
>- 如果模型转换时，用户设置了[--dynamic\_batch\_size](../CLI_options/--dynamic_batch_size.md)动态Batch档位参数，同时又通过[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数配置了动态AIPP功能：
> 实际推理时，调用**aclmdlSetInputAIPP**接口，设置动态AIPP相关参数值时，需确保batch\_size要设置为最大Batch数。接口详细说明请参见[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)。
>- 如果模型转换时，用户设置了[--dynamic\_image\_size](../CLI_options/--dynamic_image_size.md)动态分辨率参数，同时又通过[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数配置了动态AIPP功能：
> 实际推理时，调用**aclmdlSetInputAIPP**接口，设置动态AIPP相关参数值时，不能开启Crop和Padding功能。该场景下，还需要确保通过aclmdlSetInputAIPP接口设置的宽和高与**aclmdlSetDynamicHWSize**接口设置的宽、高相等，都必须设置成动态分辨率最大档位的宽、高。接口详细说明请参见[模型执行](../../../api/graph_engine_api/c/acl/model_execute_APIs.md)章节。
>- 如果模型转换时，用户设置了[--input\_shape](../CLI_options/--input_shape.md)动态shape范围参数，同时又通过[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数配置了AIPP功能，则AIPP输出的宽和高要在[--input\_shape](../CLI_options/--input_shape.md)所设置的范围内。

动态AIPP场景下，用户无需手动配置csc\_switch、rbuv\_swap\_switch等参数，根据如下配置文件配置好相关参数后，模型转换时，ATC会为动态AIPP新增一个模型输入（以下简称AippData）。

实际推理时，需要调用**aclmdlSetInputAIPP**接口，设置动态AIPP相关参数值，然后传给上述新增的AippData，AippData根据传入的参数值构造的结构体为[动态AIPP的参数输入结构](#动态aipp的参数输入结构)，该结构体无需用户手动处理。接口详细说明请参见[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)。

```textproto
aipp_op
{
    aipp_mode: dynamic
    related_input_rank: 0       # 标识对第1个输入进行AIPP处理
    max_src_image_size: 752640  # 输入图像最大的size，参数必填
}
aipp_op
{
    aipp_mode: dynamic
    related_input_rank: 1         # 标识对第2个输入进行AIPP处理
    max_src_image_size: 752640    # 输入图像最大的size，参数必填
}
```

### 动态AIPP的参数输入结构

根据[配置示例](#配置示例)配置好动态AIPP文件后，模型推理时为动态AIPP新增模型输入（AippData）传入参数值后，自动形成的结构体如下，该结构体无需用户手动处理：

```c++
typedef struct tagAippDynamicBatchPara
{
    int8_t cropSwitch;              //crop switch
    int8_t scfSwitch;               //resize switch
    int8_t paddingSwitch;   // 0: unable padding,
                           // 1: padding config value,sfr_filling_hblank_ch0 ~    sfr_filling_hblank_ch2
                          // 2: padding source picture data, single row/column copy
                          // 3: padding source picture data, block copy
                          // 4: padding source picture data, mirror copy
    int8_t rotateSwitch;  //rotate switch, 0: non-rotate, 1: rotate 90°clockwise, 2: rotate 180°clockwise, 3: rotate 270° clockwise
    int8_t reserve[4];
    int32_t cropStartPosW;          //the start horizontal position of cropping
    int32_t cropStartPosH;          //the start vertical position of cropping
    int32_t cropSizeW;              //crop width
    int32_t cropSizeH;              //crop height
    int32_t scfInputSizeW;          //input width of scf
    int32_t scfInputSizeH;          //input height of scf
    int32_t scfOutputSizeW;         //output width of scf
    int32_t scfOutputSizeH;         //output height of scf
    int32_t paddingSizeTop;         //top padding size
    int32_t paddingSizeBottom;      //bottom padding size
    int32_t paddingSizeLeft;        //left padding size
    int32_t paddingSizeRight;       //right padding size
    int16_t dtcPixelMeanChn0;       //mean value of channel 0
    int16_t dtcPixelMeanChn1;       //mean value of channel 1
    int16_t dtcPixelMeanChn2;       //mean value of channel 2
    int16_t dtcPixelMeanChn3;       //mean value of channel 3
    uint16_t dtcPixelMinChn0;       //min value of channel 0
    uint16_t dtcPixelMinChn1;       //min value of channel 1
    uint16_t dtcPixelMinChn2;       //min value of channel 2
    uint16_t dtcPixelMinChn3;       //min value of channel 3
    uint16_t dtcPixelVarReciChn0;   //sfr_dtc_pixel_variance_reci_ch0
    uint16_t dtcPixelVarReciChn1;   //sfr_dtc_pixel_variance_reci_ch1
    uint16_t dtcPixelVarReciChn2;   //sfr_dtc_pixel_variance_reci_ch2
    uint16_t dtcPixelVarReciChn3;   //sfr_dtc_pixel_variance_reci_ch3
    int8_t reserve1[16];            //32B assign, for ub copy
}kAippDynamicBatchPara;
typedef struct tagAippDynamicPara
{
    uint8_t inputFormat;        //input format: YUV420SP_U8/XRGB8888_U8/RGB888_U8
    //uint8_t outDataType; //output data type: CC_DATA_HALF,CC_DATA_INT8, CC_DATA_UINT8
    int8_t cscSwitch;               //csc switch
    int8_t rbuvSwapSwitch;          //rb/uv swap switch
    int8_t axSwapSwitch;            //RGBA->ARGB, YUVA->AYUV swap switch
    int8_t batchNum;                //batch parameter number
    int8_t reserve1[3];
    int32_t srcImageSizeW;          //source image width
    int32_t srcImageSizeH;          //source image height
    int16_t cscMatrixR0C0;          //csc_matrix_r0_c0
    int16_t cscMatrixR0C1;          //csc_matrix_r0_c1
    int16_t cscMatrixR0C2;          //csc_matrix_r0_c2
    int16_t cscMatrixR1C0;          //csc_matrix_r1_c0
    int16_t cscMatrixR1C1;          //csc_matrix_r1_c1
    int16_t cscMatrixR1C2;          //csc_matrix_r1_c2
    int16_t cscMatrixR2C0;          //csc_matrix_r2_c0
    int16_t cscMatrixR2C1;          //csc_matrix_r2_c1
    int16_t cscMatrixR2C2;          //csc_matrix_r2_c2
    int16_t reserve2[3];
    uint8_t cscOutputBiasR0;   //output bias for RGB to YUV, element of row 0, unsigned number
    uint8_t cscOutputBiasR1;   //output bias for RGB to YUV, element of row 1, unsigned number
    uint8_t cscOutputBiasR2;   //output bias for RGB to YUV, element of row 2, unsigned number
    uint8_t cscInputBiasR0;    //input bias for YUV to RGB, element of row 0, unsigned number
    uint8_t cscInputBiasR1;    //input bias for YUV to RGB, element of row 1, unsigned number
    uint8_t cscInputBiasR2;    //input bias for YUV to RGB, element of row 2, unsigned number
    uint8_t reserve3[2];
    int8_t reserve4[16];            //32B assign, for ub copy
    kAippDynamicBatchPara aippBatchPara;  //allow transfer several batch para.
} kAippDynamicPara;
```
