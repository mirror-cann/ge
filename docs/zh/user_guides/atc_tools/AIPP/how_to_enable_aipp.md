# 如何开启AIPP

通过在模型转换过程中开启AIPP功能，可以在推理之前就完成所有的数据处理；由于用的是专门的加速模块实现并保证性能，从而可以不让图像处理成为推理阶段的瓶颈，图像处理方式比较灵活。本章节给出如何在模型转换阶段开启AIPP功能。

本章节以TensorFlow框架ResNet50网络模型为例，演示如何通过模型转换开启静态AIPP功能，开启AIPP功能后，若实际提供给模型推理的测试图片不满足要求（包括图片格式，图片尺寸等），经过模型转换后，会输出满足模型要求的图片，并将该信息固化到转换后的离线模型中（模型转换后AIPP功能会以Aipp算子形式插入离线模型中）。

ResNet50网络模型要求的图片格式为RGB，图片尺寸为224\*224，另外，假设提供给模型推理的测试图片尺寸为250\*250，图片格式为YUV420SP，有效数据区域从左上角\(0, 0\)像素开始，开启AIPP过程中所需操作如[表1](#table1)分析所示。

**表 1**  场景分析<a id="table1"></a>

| 分类 | ResNet50网络模型要求 | 实际提供给模型推理的测试图片 | 所需操作 |
| --- | --- | --- | --- |
| 图片格式 | RGB | YUV420SP | 该场景下需要开启AIPP的色域转换功能，将YUV420SP格式转成模型要求的RGB格式，关于色域转换功能详细说明请参见[色域转换配置说明](CSC_config.md)。 |
| 图片尺寸 | 224*224 | 250*250 | 提供的测试图片尺寸250*250大于224*224，该场景下需要开启AIPP抠图功能，并且抠图起始位置水平、垂直方向坐标load_start_pos_h、load_start_pos_w为0，执行推理时，将从(0, 0)点开始选取224*224区域的数据。 |

详细实现步骤如下：

1. 获取TensorFlow网络模型。

    单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络的模型文件（\*.pb），并以CANN软件包运行用户将获取的文件上传至开发环境任意目录，例如上传到$HOME_/module__/_目录下。

2. 构造AIPP配置文件_insert\_op.cfg_。

    静态AIPP配置模板主要由如下几部分组成：AIPP配置模式（静态AIPP或者动态AIPP），原始图片信息（包括图片格式，以及图片尺寸），改变图片尺寸（抠图，补边）、色域转换功能等，如下分别介绍如何进行配置。

    1. AIPP配置模式由aipp\_mode参数决定，静态场景下的配置示例如下：

        ```textproto
               aipp_mode : static           #static表示配置为静态AIPP
        ```

    2. 配置原始图片信息。

        ```textproto
               input_format : YUV420SP_U8     #输入给AIPP的原始图片格式
               src_image_size_w : 250         #输入给AIPP的原始图片宽高
               src_image_size_h : 250
        ```

    3. 改变图片尺寸。

        改变图片尺寸由抠图和补边等功能完成，本示例需要配置抠图起始位置，抠图后的图片大小等信息，若抠图后图片尺寸仍旧不满足模型要求，还需要配置补边功能。

        而AIPP提供了更为方便的配置方式，就是若开启抠图功能，并且不配置补边功能，抠图大小可以取值为0或者不配置，此时抠图大小的宽和高来自模型--input\_shape中的宽和高。本示例中我们不配置抠图大小，配置示例如下：

        ```textproto
               crop: true                     #抠图开关，用于改变图片尺寸
               load_start_pos_h: 0            #抠图起始位置水平、垂直方向坐标
               load_start_pos_w: 0
        ```

    4. 色域转换功能。

        色域转换功能由csc\_switch参数控制，并通过色域转换系数matrix\_r\*c\*、通道交换rbuv\_swap\_switch等参数配合使用。AIPP提供了一个比较方便的功能，就是一旦确认了AIPP处理前与AIPP处理后的图片格式，即可确定色域转换相关的参数值，**用户无需修改**，即上述参数都可以直接从模板中进行复制，模板示例以及更多配置模板请参见[色域转换配置说明](CSC_config.md)。如下为该场景下的配置示例：

        ```textproto
               csc_switch : true              #色域转换开关，true表示开启色域转换
               rbuv_swap_switch : false       #通道交换开关（R通道与B通道交换开关/U通道与V通道交换），本例中不涉及两个通道的交换，故设置为false，默认为false
               matrix_r0c0 : 256              #色域转换系数
               matrix_r0c1 : 0
               matrix_r0c2 : 359
               matrix_r1c0 : 256
               matrix_r1c1 : -88
               matrix_r1c2 : -183
               matrix_r2c0 : 256
               matrix_r2c1 : 454
               matrix_r2c2 : 0
               input_bias_0 : 0
               input_bias_1 : 128
               input_bias_2 : 128
        ```

    将上述所有的参数组合到_insert\_op.cfg_文件中，即为我们需要构造的AIPP配置文件，完整示例如下：

    ```textproto
    aipp_op {
           aipp_mode : static             #AIPP配置模式
           input_format : YUV420SP_U8     #输入给AIPP的原始图片格式
           src_image_size_w : 250         #输入给AIPP的原始图片宽高
           src_image_size_h : 250
           crop: true                     #抠图开关，用于改变图片尺寸
           load_start_pos_h: 0            #抠图起始位置水平、垂直方向坐标
           load_start_pos_w: 0
           csc_switch : true              #色域转换开关，true表示开启色域转换
           rbuv_swap_switch : false       #通道交换开关
           matrix_r0c0 : 256              #色域转换系数，用户无需修改
           matrix_r0c1 : 0
           matrix_r0c2 : 359
           matrix_r1c0 : 256
           matrix_r1c1 : -88
           matrix_r1c2 : -183
           matrix_r2c0 : 256
           matrix_r2c1 : 454
           matrix_r2c2 : 0
           input_bias_0 : 0
           input_bias_1 : 128
           input_bias_2 : 128
    }
    ```

    您可以根据[AIPP配置示例](aipp_config_example.md)或[典型场景样例参考](typical_scenario_examples.md)章节获取更多场景AIPP配置示例，如果上述示例仍旧无法满足要求，则需要参见[配置文件模板](config_file_template.md)自行构造配置文件。将上述_insert\_op.cfg_文件上传到ATC工具所在Linux服务器。

3. atc命令中加入[--insert\_op\_conf](../CLI_options/--insert_op_conf.md)参数，用于插入aipp预处理算子，执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow.pb --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version> --insert_op_conf=$HOME/module/insert_op.cfg
    ```

    关于参数的详细解释以及使用方法请参见[参数说明](../CLI_options/README.md)。若提示如下信息，则说明模型转换成功。

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：*tf\_resnet50*.om）。

4. （可选）如果用户想查看转换后离线模型中Aipp算子的相关信息，则可以将上述离线模型转成JSON文件查看，命令如下：

    ```bash
    atc --mode=1 --om=$HOME/module/out/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json
    ```

    如下为JSON文件中带有aipp信息的样例（如下样例中所有aipp属性值都为样例，请以用户实际构造的配置文件为准）：

    ```json
    {
                  "key": "aipp",
                  "value": {
                    "func": {
                      "attr": [
                        {
                          "key": "mean_chn_0",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "mean_chn_1",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "mean_chn_2",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "mean_chn_3",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "csc_switch",
                          "value": {
                            "b": true
                          }
                        },
                        {
                          "key": "input_format",
                          "value": {
                            "i": 1
                          }
                        },
                        {
                          "key": "input_bias_0",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "input_bias_1",
                          "value": {
                            "i": 128
                          }
                        },
                        {
                          "key": "input_bias_2",
                          "value": {
                            "i": 128
                          }
                        },
                        {
                          "key": "aipp_mode",
                          "value": {
                            "i": 1
                          }
                        },
                        {
                          "key": "src_image_size_h",
                          "value": {
                            "i": 250
                          }
                        },
                        {
                          "key": "crop_size_h",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "matrix_r0c0",
                          "value": {
                            "i": 256
                          }
                        },
                        {
                          "key": "matrix_r0c1",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "matrix_r0c2",
                          "value": {
                            "i": 359
                          }
                        },
                        {
                          "key": "src_image_size_w",
                          "value": {
                            "i": 250
                          }
                        },
                        {
                          "key": "crop_size_w",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "rbuv_swap_switch",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "padding",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "ax_swap_switch",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "top_padding_size",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "matrix_r1c0",
                          "value": {
                            "i": 256
                          }
                        },
                        {
                          "key": "matrix_r1c1",
                          "value": {
                            "i": -88
                          }
                        },
                        {
                          "key": "matrix_r1c2",
                          "value": {
                            "i": -183
                          }
                        },
                        {
                          "key": "resize",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "resize_output_h",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "related_input_rank",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "load_start_pos_h",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "matrix_r2c0",
                          "value": {
                            "i": 256
                          }
                        },
                        {
                          "key": "matrix_r2c1",
                          "value": {
                            "i": 454
                          }
                        },
                        {
                          "key": "matrix_r2c2",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "resize_output_w",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "var_reci_chn_0",
                          "value": {
                            "f": "1"
                          }
                        },
                        {
                          "key": "var_reci_chn_1",
                          "value": {
                            "f": "1"
                          }
                        },
                        {
                          "key": "var_reci_chn_2",
                          "value": {
                            "f": "1"
                          }
                        },
                        {
                          "key": "load_start_pos_w",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "var_reci_chn_3",
                          "value": {
                            "f": "1"
                          }
                        },
                        {
                          "key": "single_line_mode",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "output_bias_0",
                          "value": {
                            "i": 16
                          }
                        },
                        {
                          "key": "output_bias_1",
                          "value": {
                            "i": 128
                          }
                        },
                        {
                          "key": "output_bias_2",
                          "value": {
                            "i": 128
                          }
                        },
                        {
                          "key": "right_padding_size",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "bottom_padding_size",
                          "value": {
                            "i": 0
                          }
                        },
                        {
                          "key": "min_chn_0",
                          "value": {
                            "f": 0
                          }
                        },
                        {
                          "key": "min_chn_1",
                          "value": {
                            "f": 0
                          }
                        },
                        {
                          "key": "min_chn_2",
                          "value": {
                            "f": 0
                          }
                        },
                        {
                          "key": "min_chn_3",
                          "value": {
                            "f": 0
                          }
                        },
                        {
                          "key": "crop",
                          "value": {
                            "b": false
                          }
                        },
                        {
                          "key": "cpadding_value",
                          "value": {
                            "f": 0
                          }
                        },
                        {
                          "key": "left_padding_size",
                          "value": {
                            "i": 0
                          }
                        }
                      ]
                    }
                  }
                }
    ```
