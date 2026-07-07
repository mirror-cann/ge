# 典型场景样例参考

## YUV400\_U8转GRAY格式

- **场景说明**：

    AIPP输入图像格式为YUV400\_U8、输出图像格式为GRAY，输入图像尺寸为224\*224，有效数据区域从左上角\(0, 0\)像素开始；原始网络模型的C=1，H和W均为220。

- **该场景下涉及以下AIPP配置**：
  - 开启抠图功能参数crop；
  - 抠图起始位置水平、垂直方向坐标load\_start\_pos\_h、load\_start\_pos\_w为0；
  - 无需配置crop\_size\_w和crop\_size\_h参数，此时抠图大小（crop\_size\[W|H\]）的宽和高取值来自模型转换时**--input\_shape**参数中的宽和高，将从\(0, 0\)点开始选取220\*220区域的数据；
  - 无需配置色域转换开关csc\_switch，并且对于同一个原始网络模型，如果AIPP输入的是YUV420SP\_U8图像，则可以使用同一套AIPP配置，即只取了Y通道的数据。

- **AIPP配置文件示例如下：**

    ```textproto
    aipp_op{
        aipp_mode: static
        csc_switch: false
        crop: true
        input_format: YUV400_U8
        load_start_pos_h: 0
        load_start_pos_w: 0
        src_image_size_w: 224
        src_image_size_h: 224
        # 归一化系数需要根据用户模型实际需求配置，如下所列常见值仅作为示例
        mean_chn_0: 128
        min_chn_0: 0.0
        var_reci_chn_0: 0.00390625
    }
    ```

## YUV420SP\_U8转BGR格式

- **场景说明：**

    AIPP输入图像格式为YUV420SP\_U8（NV12）、输出图像格式为BGR，输入图像尺寸为256\*256；原始网络模型的C=3，H和W与AIPP输入图像尺寸相同。

- **该场景涉及以下AIPP配置：**
  - 无需配置抠图功能参数crop；
  - 需要配置色域转换开关csc\_switch和相应的CSC矩阵参数。

- **AIPP配置文件示例如下：**

    ```textproto
    aipp_op {
        aipp_mode: static
        input_format: YUV420SP_U8
        csc_switch: true
        # 如果输入的是YVU420SP_U8（NV21）图像，则需要将rbuv_swap_switch参数设置为true
        rbuv_swap_switch: false
        related_input_rank: 0
        src_image_size_w: 256
        src_image_size_h: 256
        crop: false
        matrix_r0c0: 298
        matrix_r0c1: 516
        matrix_r0c2: 0
        matrix_r1c0: 298
        matrix_r1c1: -100
        matrix_r1c2: -208
        matrix_r2c0: 298
        matrix_r2c1: 0
        matrix_r2c2: 409
        input_bias_0: 16
        input_bias_1: 128
        input_bias_2: 128
        # 归一化系数需要根据用户模型实际需求配置，如下所列常见值仅作为示例
        # 归一化系数应用于色域转换和通道交换之后的通道
        mean_chn_0: 104
        mean_chn_1: 117
        mean_chn_2: 123
        min_chn_0: 0.0
        min_chn_1: 0.0
        min_chn_2: 0.0
        var_reci_chn_0: 1.0
        var_reci_chn_1: 1.0
        var_reci_chn_2: 1.0
    }
    ```

## RGB888\_U8转RGB（或BGR）格式

- **场景说明：**

    AIPP输入图像格式为RGB888\_U8、输出图像格式为RGB，输入图像尺寸为250\*250，有效数据区域从左上角\(0, 0\)像素开始；原始网络模型的C=3，H和W均为240。

- **该场景下涉及以下AIPP配置：**
  - 开启抠图功能参数crop；
  - 抠图起始位置水平、垂直方向坐标load\_start\_pos\_h、load\_start\_pos\_w为0；
  - 无需配置crop\_size\_w和crop\_size\_h参数，此时抠图大小（crop\_size\[W|H\]）的宽和高取值来自模型转换时**--input\_shape**参数中的宽和高，将从\(0, 0\)点开始选取240\*240区域的数据；
  - 无需配置通道交换开关参数rbuv\_swap\_switch、色域转换开关参数csc\_switch和CSC矩阵参数。

- **AIPP配置文件示例如下：**

    ```textproto
    aipp_op {
        aipp_mode: static
        input_format: RGB888_U8
        csc_switch: false
        related_input_rank: 0
        src_image_size_w: 250
        src_image_size_h: 250
        crop: true
        load_start_pos_w: 0
        load_start_pos_h: 0
        # 如果原始模型需要的是BGR格式，则需要将rbuv_swap_switch参数设置为true
        rbuv_swap_switch: false
        # 归一化系数需要根据用户模型实际需求配置，此处取默认值，即不改变像素的值
        # 若配置归一化系数，将应用于通道交换之后的通道
    }
    ```
