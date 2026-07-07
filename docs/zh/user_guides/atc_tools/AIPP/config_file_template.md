# 配置文件模板

AIPP配置文件通过本章节给出的模板进行配置，内容需要满足prototxt格式，用户根据场景决定配置哪些参数，修改为合适的取值另存后供模型转换使用；使用配置模板之前需要先查看相关约束。

## 模板使用整体约束

- **使用配置文件模板时，请将需要配置的参数去注释，并改为合适的值。**
- **模板中参数取值都为默认值，实际使用时，如果配置文件中某些参数未配置，则模型转换时自动设置成该模板中相应参数的默认值。**
- **静态AIPP场景下，input\_format属性为必选属性，其余属性均为可选配置，如果未配置，则模型转换时自动设置成该模板中相应参数的默认值。**
- **由于硬件处理逻辑的限制，配置文件中的参数有如下处理顺序要求：通道交换（rbuv\_swap\_switch）\>图像裁剪（crop ）\> 色域转换（通道交换） \> 数据减均值/归一化 \> 图像边缘填充（padding）。**
- AIPP当前支持色域转换、图像裁剪、减均值、乘系数、通道交换、单行模式的能力，**输入图片的类型仅支持RAW和UINT8格式**。

<!-- npu="950,A3,910b,910,310p,310b" id3 -->
- 抠图约束如下：
  - 若input_format取值为YUV420SP_U8，load_start_pos_w、load_start_pos_h必须为偶数，Ascend 950PR/Ascend 950DT无偶数要求；若input_format取值为其他值，对load_start_pos_w、load_start_pos_h无约束。
  - 若开启抠图功能，则src_image_size[W|H] >= crop_size[W|H]+load_start_pos[W|H]。
<!-- end id3 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/AIPP/config_file_template_res.md#id3 -->

- padding约束：
  - AIPP经过padding后，输出的H和W要与模型需要的H和W保持一致。
  <!-- @ref: ge/res/docs/zh/user_guides/atc_tools/AIPP/config_file_template_res.md#id4 -->
  <!-- npu="910,310p" id4 -->
  - 针对Atlas 推理系列产品、Atlas 训练系列产品，W取值要<=1080。
  <!-- end id4 -->
  <!-- npu="950,A3,910b,310b" id5 -->
  - 针对Atlas 200I/500 A2 推理产品、Atlas A2 训练系列产品/Atlas A2 推理系列产品、Atlas A3 训练系列产品/Atlas A3 推理系列产品、Ascend 950PR/Ascend 950DT，W取值要<=4096。
  <!-- end id5 -->
<!-- npu="950,A3,910b,910,310p,310b" id1 -->
- 该版本不支持raw_rgbir_to_f16_n配置项。
<!-- end id1 -->
- 若输入图片为RGB（由R、G、B三个分量组成的图片），其对应的输入、输出通道顺序，从高地址到低地址依次为：\{R,G,B\}。

- 动态AIPP的参数每次推理需要计算，计算需要耗时，所以动态AIPP的性能比静态AIPP性能要差。

- 经过AIPP处理后的图片，统一采用NC1HWC0的五维数据格式进行存储：

  以原始模型要求的图片为RGB（由R、G、B三个分量组成的图片）为例进行说明，配置了AIPP功能场景下：

  - Caffe/ONNX框架数据格式只能设置为NCHW（数据存储格式为RRRRRRGGGGGGBBBBBB）
  - TensorFlow框架数据格式只能设置为NHWC（数据存储格式为RGBRGBRGBRGBRGBRGB）或NCHW（数据存储格式为RRRRRRGGGGGGBBBBBB）。

    实际提供的图片经过AIPP色域转换功能处理后，输出的离线模型中图片为RGB，并以NC1HWC0五维数据格式进行存储（关于NC1HWC0详细介绍请参见[关键概念](../overview/atc_overview.md#关键概念)）：若AIPP输出数据类型为FP16，则C0=16，从高位到低位依次为R,G,B，其余位数补0；C1=1。

- 模型转换是否开启AIPP功能，执行推理业务时，对输入图片数据的要求：
  - 模型转换时开启AIPP：在进行推理业务时，输入图片数据要求为NHWC排布，该场景下最终与AIPP连接的输入节点的格式被强制改成NHWC，可能与模型转换命令中[--input\_format](../CLI_options/--input_format.md)参数指定的格式不一致。
  - 模型转换时没有开启AIPP：在进行推理业务时，模型的Format需与输入图片的Format保持一致。例如，输入图片的Format为NHWC，但某模型默认的Format为NCHW，此时输入图片和模型的Format不一致，用户可在模型转换时指定[--input\_format](../CLI_options/--input_format.md)调整模型的Format，也可以选择符合模型要求的输入图片。

- 对于输入图像格式YUV420SP，根据UV分量顺序不同，YUV420SP又分为YUV420SP\_UV\(NV12\)和YUV420SP\_VU\(NV21\)，分别对应[色域转换配置说明](CSC_config.md)中的YUV420SP\_U8、YVU420SP\_U8，默认为YUV420SP\_UV\(NV12\)。

  对于AIPP配置文件中的input\_format参数，需始终配置为NV12格式（YUV420SP\_U8），通过rbuv\_swap\_switch参数控制实际提供给AIPP的图片格式：

  - 若rbuv\_swap\_switch设置为false，则实际提供的图片格式为YUV420SP\_U8。
  - 若rbuv\_swap\_switch设置为true，则实际提供的图片格式为YVU420SP\_U8。

- AIPP不同图像格式对应C轴取值约束。

    **表 1**  不同图像格式对应C轴取值

    | AIPP的输入图像格式(input_format) | C轴取值 |
    | --- | --- |
    | YUV420SP_U8 | C=1 |
    | RGB888_U8 | C=3 |
    | XRGB8888_U8 | C=4 |
    | YUV400_U8 | C=1 |

- AIPP针对各种图像格式的典型参数配置如下表所示。

    **表 2**  各种图像格式的典型参数配置

    | AIPP的输入图像格式(input_format) | 输入图像内存排布格式 | 对应原始输入图像格式 | AIPP输出格式 | 关于AIPP配置文件中相关参数的说明 |
    | --- | --- | --- | --- | --- |
    | RGB888_U8 | NHWC | RGB package | NC1HWC0 | rbuv_swap_switch通常设置为false |
    | RGB888_U8 | NHWC | BGR package | NC1HWC0 | rbuv_swap_switch通常设置为true，内部先转为RGB package再做后续处理 |
    | YUV420SP_U8 | / | YUV420 sp NV12 8bit | NC1HWC0 | rbuv_swap_switch通常设置为false |
    | YUV420SP_U8 | / | YUV420 sp NV21 8bit | NC1HWC0 | rbuv_swap_switch通常设置为true，内部先转为NV12格式再做后续处理 |
    | YUV400_U8 | NHWC | 灰度图 | NC1HWC0 | / |
    | XRGB8888_U8 | NHWC | - XRGB package<br>  - XBGR package<br>  - RGBX package<br>  - BGRX package | NC1HWC0 | - XRGB packagerbuv_swap_switch通常设置为false，ax_swap_switch通常设置为true<br>  - XBGR packagerbuv_swap_switch通常设置为true，ax_swap_switch通常设置为true，内部先转为XRGB package再做后续处理<br>  - RGBX packagerbuv_swap_switch通常设置为false，ax_swap_switch通常设置为false，内部先转为XRGB package再做后续处理<br>  - BGRX packagerbuv_swap_switch通常设置为true，ax_swap_switch通常设置为false，内部先转为XRGB package再做后续处理 |

## 配置文件模板

```textproto
# AIPP的配置以aipp_op开始，标识这是一个AIPP算子的配置，aipp_op支持配置多个
aipp_op {

#========================= 全局设置（start） ===========================================================================================================================================================
# aipp_mode指定了AIPP的模式，必须配置
# 类型：enum
# 取值范围：dynamic/static，dynamic表示动态AIPP，static表示静态AIPP
aipp_mode:

# related_input_rank参数为可选，标识对模型的第几个输入做AIPP处理，从0开始，默认为0。例如模型有两个输入，需要对第2个输入做AIPP，则配置related_input_rank为1。
# 类型：整型
# 配置范围 >= 0
related_input_rank: 0

# related_input_name参数为可选，标识对模型的第几个输入做AIPP处理，此处需要填写为模型输入的name（input对应的值）或者模型首层节点的输出（top参数对应的取值）。该参数只适用于Caffe网络模型，且不能与related_input_rank参数同时使用；如果同时配置，related_input_name优先级高于related_input_rank。
# 例如模型有两个输入，且输入name分别为data、im_info，需要对第二个输入做AIPP，则配置related_input_name为im_info。
# 类型：string
# 配置范围：无
related_input_name: ""

#========================= 全局设置（end） =============================================================================================================================================================

#========================= 动态AIPP需设置，静态AIPP无需设置（start） ===================================================================================================================================
# 输入图像最大的size，动态AIPP必须配置（如果为动态batch场景，N为最大档位数的取值）
# 类型：int
max_src_image_size: 0
# 若输入图像格式为YUV400_U8，则max_src_image_size>=N * src_image_size_w * src_image_size_h * 1。
# 若输入图像格式为YUV420SP_U8，则max_src_image_size>=N * src_image_size_w * src_image_size_h * 1.5。
# 若输入图像格式为XRGB8888_U8，则max_src_image_size>=N * src_image_size_w * src_image_size_h * 4。
# 若输入图像格式为RGB888_U8，则max_src_image_size>=N * src_image_size_w * src_image_size_h * 3。

# 是否支持旋转，保留字段，暂不支持该功能
# 类型：bool
# 取值范围：true/false，true表示支持旋转，false表示不支持旋转
support_rotation: false
#========================= 动态AIPP需设置，静态AIPP无需设置（end） =======================================================================================================================================

#========================= 静态AIPP需设置，动态AIPP无需设置（start）======================================================================================================================================
# 输入图像格式，必选
# 类型: enum
# 取值范围：YUV420SP_U8、XRGB8888_U8、RGB888_U8、YUV400_U8

input_format:
# 说明：模型转换完毕后，在对应的om离线模型文件中，上述参数分别以1、2、3、4枚举值呈现。

# 原始图像的宽度、高度
# 类型：int32
# 取值范围&约束：宽度取值范围为[2,4096]或0；高度取值范围为[1,4096]或0，对于YUV420SP_U8类型的图像，要求原始图像的宽和高取值是偶数
src_image_size_w: 0
src_image_size_h: 0
# 说明：请根据实际图片的宽、高配置src_image_size_w和src_image_size_h；只有crop，padding功能都没有开启的场景，src_image_size_w和src_image_size_h才能取值为0或不配置，该场景下会取网络模型输入定义的w和h，并且网络模型输入定义的w取值范围为[2,4096]，h取值范围为[1,4096]。

# C方向的填充值，保留字段，暂不支持该功能
# 类型： float16
# 取值范围：[-65504, 65504]
cpadding_value: 0.0

#========= crop参数设置（配置样例请参见AIPP配置 > Crop/Padding配置说明） =========
# AIPP处理图片时是否支持抠图
# 类型：bool
# 取值范围：true/false，true表示支持，false表示不支持
crop: false

# 抠图起始位置水平、垂直方向坐标，抠图大小为网络输入定义的w和h
# 类型：int32
# 取值范围&约束： [0,4095]
# 说明：load_start_pos_w<src_image_size_w，load_start_pos_h<src_image_size_h
load_start_pos_w: 0
load_start_pos_h: 0

# 抠图后的图像size
# 类型：int32
# 取值范围&约束： [0,4096]，load_start_pos_w + crop_size_w <= src_image_size_w、load_start_pos_h + crop_size_h <= src_image_size_h
crop_size_w: 0
crop_size_h: 0
说明：若开启抠图功能，并且没有配置padding，该场景下crop_size_w和crop_size_h才能取值为0或不配置，此时抠图大小（crop_size[W|H]）的宽和高取值来自模型文件--input_shape中的宽和高，并且--input_shape中的宽和高取值范围为[1,4096]。

#================================== resize参数设置 ================================
# AIPP处理图片时是否支持缩放，保留字段，暂不支持该功能
# 类型：bool
# 取值范围：true/false，true表示支持，false表示不支持
resize: false

# 缩放后图像的宽度和高度，保留字段，暂不支持该功能
# 类型：int32
# 取值范围&约束：resize_output_h：[16,4096]或0；resize_output_w：[16,1920]或0；resize_output_w/resize_input_w∈[1/16,16]、resize_output_h/resize_input_h∈[1/16,16]
resize_output_w: 0
resize_output_h: 0
# 说明：若开启了缩放功能，并且没有配置padding，该场景下resize_output_w和resize_output_h才能取值为0或不配置，此时缩放后图像的宽和高取值来自模型文件--input_shape中的宽和高，并且--input_shape中的高取值范围为[16,4096]，宽取值范围为[16,1920]。


#======== padding参数设置（配置样例请参见AIPP配置 > Crop/Padding配置说明） =========
# AIPP处理图片时padding开启开关
# 类型：bool
# 取值范围：true/false，true表示支持，false表示不支持
padding: false

# H和W的填充值，静态AIPP配置
# 类型： int32
# 取值范围：[0,32]
left_padding_size: 0
right_padding_size: 0
top_padding_size: 0
bottom_padding_size: 0
# 说明：AIPP经过padding后，输出的H和W要与模型需要的H和W保持一致


# 上下左右方向上padding的像素取值，静态AIPP配置
# 类型：uint8/int8/float16
# 取值范围分别为：[0,255]、[-128, 127]、[-65504, 65504]
padding_value: 0
# 说明：该参数取值需要与最终AIPP输出图片的数据类型保持一致。


#================================ rotation参数设置 ==================================
# AIPP处理图片时的旋转角度，保留字段，暂不支持该功能
# 类型：uint8
# 范围：{0, 1, 2, 3} 0不旋转，1顺时针90°，2顺时针180°，3顺时针270°
rotation_angle: 0


#========= 色域转换参数设置（配置样例请参见AIPP配置 > 色域转换配置说明） =============
# 色域转换开关，静态AIPP配置
# 类型：bool
# 取值范围：true/false，true表示开启色域转换，false表示关闭
csc_switch: false

# R通道与B通道交换开关/U通道与V通道交换开关
# 类型：bool
# 取值范围：true/false，true表示开启通道交换，false表示关闭
rbuv_swap_switch: false

# RGBA->ARGB, YUVA->AYUV交换开关
# 类型：bool
# 取值范围：true/false，true表示开启，false表示关闭
ax_swap_switch: false

# 单行处理模式（只处理抠图后的第一行）开关，保留字段，暂不支持该功能
# 类型：bool
# 取值范围：true/false，true表示开启单行处理模式，false表示关闭
single_line_mode: false

# 若色域转换开关为false，则本功能不起作用。
# 若输入图片通道数为4，则忽略A通道或X通道。
# YUV转BGR：
# | B |   | matrix_r0c0 matrix_r0c1 matrix_r0c2 | | Y - input_bias_0 |
# | G | = | matrix_r1c0 matrix_r1c1 matrix_r1c2 | | U - input_bias_1 | >> 8
# | R |   | matrix_r2c0 matrix_r2c1 matrix_r2c2 | | V - input_bias_2 |
# BGR转YUV：
# | Y |   | matrix_r0c0 matrix_r0c1 matrix_r0c2 | | B |        | output_bias_0 |
# | U | = | matrix_r1c0 matrix_r1c1 matrix_r1c2 | | G | >> 8 + | output_bias_1 |
# | V |   | matrix_r2c0 matrix_r2c1 matrix_r2c2 | | R |        | output_bias_2 |

# 3*3 CSC矩阵元素
# 类型：int16
# 取值范围：[-32677 ,32676]
matrix_r0c0: 298
matrix_r0c1: 516
matrix_r0c2: 0
matrix_r1c0: 298
matrix_r1c1: -100
matrix_r1c2: -208
matrix_r2c0: 298
matrix_r2c1: 0
matrix_r2c2: 409

# RGB转YUV时的输出偏移
# 类型：uint8
# 取值范围：[0, 255]
output_bias_0: 16
output_bias_1: 128
output_bias_2: 128

# YUV转RGB时的输入偏移
# 类型：uint8
# 取值范围：[0, 255]
input_bias_0: 16
input_bias_1: 128
input_bias_2: 128


#============================== 减均值、乘系数设置 =================================
# 计算规则如下：
# 当uint8->uint8时，本功能不起作用
# 当uint8->fp16时，pixel_out_chx(i) = [pixel_in_chx(i) – mean_chn_i – min_chn_i] * var_reci_chn_i

# 每个通道的均值
# 类型：uint8
# 取值范围：[0, 255]
mean_chn_0: 0
mean_chn_1: 0
mean_chn_2: 0
mean_chn_3: 0

# 每个通道的最小值
# 类型：float16
# 取值范围：[0, 255]
min_chn_0: 0.0
min_chn_1: 0.0
min_chn_2: 0.0
min_chn_3: 0.0

# 每个通道方差的倒数
# 类型：float16
# 取值范围：[-65504, 65504]
var_reci_chn_0: 1.0
var_reci_chn_1: 1.0
var_reci_chn_2: 1.0
var_reci_chn_3: 1.0

# RGB16/RGB20/RGB24/RGB8_IR/RGB16_IR/RGB20_IR->fp16， rgbir格式转成fp16的参数
# 类型：uint32
# 取值范围：[0,31]，默认为8
raw_rgbir_to_f16_n: 8
# raw格式转成fp16的计算公式：pixel_out_chx(i) = Max((fp16(rounding(FP32(UINT24(pixel_in_chx(i)) - mean_chn_i)/(2^raw_rgbir_to_f16_n))) - min_chn_i), 0)* var_reci_chn_i


#========================= 静态AIPP需设置，动态AIPP无需设置（end）=====================================================================================================================================

}
```
