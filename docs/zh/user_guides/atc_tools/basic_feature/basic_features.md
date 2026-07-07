# 初级功能

## 原始模型文件或离线模型转成JSON文件

### 场景介绍

如果用户不方便查看原始模型或离线模型的参数信息时，可以将原始模型或离线模型转成JSON文件进行查看。

### 转换方法

本章节以TensorFlow框架ResNet50网络模型为例进行演示，单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络模型文件（\*.pb）。

- 原始模型文件转JSON文件

    命令示例如下：

    ```bash
    atc --mode=1 --om=$HOME/module/resnet50_tensorflow_1.7.pb  --json=$HOME/module/out/tf_resnet50.json  --framework=3
    ```

  - --mode：运行模式，1表示原始模型文件或离线模型转JSON，此处特指原始模型文件转JSON。
  - --om：指定**ResNet50网络模型文件**所在路径。
  - --json：转换为JSON格式的文件路径和文件名。
  - --framework：原始框架类型，3表示TensorFlow。

- 离线模型转JSON文件

    该场景的前提是用户根据[开源框架的TensorFlow网络模型转换成离线模型](../overview/quick_start.md#开源框架的tensorflow网络模型转换成离线模型)已经得到了om离线模型文件，命令示例如下：

    ```bash
    atc --mode=1 --om=$HOME/module/out/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json
    ```

  - --mode：运行模式，1表示原始模型文件或离线模型转JSON，此处特指离线模型文件转JSON。
  - --om：指定**离线模型文件**所在路径。

关于参数的详细解释请参见[参数说明](../CLI_options/README.md)。若提示如下信息，则说明转换成功。
<!-- npu="950,A3,910b,910,310p,310b" id2 -->
若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。

<!-- end id2 -->
```console
ATC run success, welcome to the next use.
```

成功执行命令后，在--json参数指定的路径下，可查看转换后的JSON文件信息，如下为部分JSON片段：

```json
{
  "node": [
    {
      "attr": [
        {
          "key": "shape",
          "value": {
            "shape": {
              "dim": [
                {
                  "size": 1
                },
                {
                  "size": 224
                },
                {
                  "size": 224
                },
                {
                  "size": 3
                }
              ]
            }
          }
        },
        {
          "key": "dtype",
          "value": {
            "type": "DT_FLOAT"
          }
        }
      ],
      "name": "Placeholder",
      "op": "Placeholder"
    },
...

  ]
}
```

## 离线模型支持动态batch\_size/动态分辨率

<!-- npu="IPV350" id1 -->
**IPV350不支持动态batch\_size和动态分辨率特性。**
<!-- end id1 -->

### 场景介绍

某些推理场景，如检测出目标后再执行目标识别网络，由于目标个数不固定导致目标识别网络输入batch\_size不固定。如果每次推理都按照最大的batch\_size或最大分辨率进行计算，会造成计算资源浪费。

为此，ATC工具提供了[--dynamic\_batch\_size](../CLI_options/--dynamic_batch_size.md)参数设置batch\_size档位；提供了[--dynamic\_image\_size](../CLI_options/--dynamic_image_size.md)参数设置分辨率档位。

### 转换方法

如下转换示例以TensorFlow框架ResNet50网络模型为例进行演示，单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络的模型文件（\*.pb）。

1. 以CANN软件包运行用户登录开发环境，将模型文件（\*.pb）上传到开发环境任意路径，例如上传到$HOME_/module__/_目录下。
2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    - 动态batch\_size

        ```bash
        atc --model=$HOME/module/resnet50_tensorflow_1.7.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version> --input_shape="Placeholder:-1,224,224,3"  --dynamic_batch_size="1,2,4,8"
        ```

    - 动态分辨率

        ```bash
        atc --model=$HOME/module/resnet50_tensorflow_1.7.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version>  --input_shape="Placeholder:1,-1,-1,3"  --dynamic_image_size="224,224;448,448"
        ```

    关键参数解释如下：

    - --dynamic\_batch\_size：设置动态batch\_size参数。
    - --dynamic\_image\_size：设置输入图片的动态分辨率参数。
    - --input\_shape：指定模型输入数据的shape，配合--dynamic\_batch\_size或--dynamic\_image\_size参数使用。
    - --model：ResNet50网络模型文件所在路径。
    - --framework：原始框架类型，3表示TensorFlow。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)。若提示如下信息，则说明模型转换成功。
    <!-- npu="950,A3,910b,910,310p,310b" id3 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id3 -->

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：tf\_resnet50.om）。

    模型转换完成后，在生成的om离线模型中，会新增一个输入（如[图1](#fig1)中红框中的Data输入），在模型推理时通过该新增的输入提供具体的batch\_size值（或分辨率值）。例如，a输入的batch\_size是动态的（或分辨率是动态的），在om离线模型中，会有与a对应的b输入来描述a的batch\_size（或分辨率取值）。

    **图 1**  包含动态batch\_size功能的离线模型<a id="fig1"></a>
    ![](../figures/offline_model_with_dynamic_batch_size.png "包含动态batch_size功能的离线模型")

## 离线模型支持动态维度

### 场景介绍

为支持Transformer等网络模型在输入Tensor维度不确定的场景，ATC工具提供了[--dynamic\_dims](../CLI_options/--dynamic_dims.md)参数实现ND格式下任意维度的档位设置。ND表示支持任意格式。

### 转换方法

本章节以TensorFlow框架ResNet50网络模型为例进行演示，单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络模型文件（\*.pb）。

1. 以CANN软件包运行用户登录开发环境，将模型文件上传到开发环境任意路径，例如上传到$HOME_/module__/_目录下。
2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow_1.7.pb --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version>  --input_shape="Placeholder:-1,-1,-1,3" --dynamic_dims="1,224,224;8,448,448" --input_format=ND
    ```

    关键参数解释如下：

    - --dynamic\_dims：设置ND格式下动态维度档位。
    - --input\_shape：指定模型输入数据的shape，配合--dynamic\_dims参数使用。
    - --input\_format：指定Format为ND格式。
    - --model：ResNet50网络模型文件所在路径。
    - --framework：原始框架类型，3表示TensorFlow。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)。若提示如下信息，则说明模型转换成功。
    <!-- npu="950,A3,910b,910,310p,310b" id4 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id4 -->

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型。

    模型转换完成后，在生成的om离线模型中，会新增一个输入（如[图1](#fig2)中红框中的Data输入），在模型推理时通过该新增的输入提供具体的维度值。例如，a输入的维度为动态的，在om离线模型中，会有与a对应的b输入来描述a的维度值。

    **图 1**  包含动态维度的离线模型<a id="fig2"></a>
    ![](../figures/offline_model_with_dynamic_dims.png "包含动态维度的离线模型")

## 自定义离线模型的输入输出数据类型

### 场景介绍

模型转换时支持指定网络的输入节点、输出节点的DataType、Format、模型转换支持精度选择等关键参数。

假如，针对TensorFlow框架ResNet-50网络模型，要求转换后离线模型的输入数据为Float16类型，指定_MaxPoolWithArgmax_算子作为输出算子（对应的节点名称为fp32\_vars/MaxPoolWithArgmax），并且指定该输出节点的数据类型为FP16。该场景下就需要分别使用[--input\_fp16\_nodes](../CLI_options/--input_fp16_nodes.md)、[--out\_nodes](../CLI_options/--out_nodes.md)、[--output\_type](../CLI_options/--output_type.md)等参数来实现上述功能。

### 转换方法

本章节以TensorFlow框架ResNet50网络模型为例进行演示，单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络模型文件（\*.pb）。

1. 以CANN软件包运行用户登录开发环境，将模型文件（\*.pb）上传到开发环境任意路径，例如上传到$HOME_/module__/_目录下。
2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow_1.7.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version>  --input_fp16_nodes="Placeholder" --out_nodes="fp32_vars/MaxPoolWithArgmax:0" --output_type="fp32_vars/MaxPoolWithArgmax:0:FP16"
    ```

    关键参数解释如下：

    - --input\_fp16\_nodes：指定输入数据类型为Float16。
    - --out\_nodes：指定MaxPoolWithArgmax算子作为模型的输出。
    - --output\_type：指定输出节点的数据类型为Float16。
    - --model：ResNet50网络模型文件所在路径。
    - --framework：原始框架类型，3表示TensorFlow。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)。若提示如下信息，则说明模型转换成功。
    <!-- npu="950,A3,910b,910,310p,310b" id5 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id5 -->

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在output参数指定的路径下，可查看离线模型（如：tf\_resnet50.om）。[图1](#fig3)为_MaxPoolWithArgmax_算子作为模型输出算子的示意图（下图使用Netron可视化软件打开）。

    **图 1**  指定某个算子为离线模型输出<a id="fig3"></a>
    ![](../figures/specify_op_as_offline_model_output.png "指定某个算子为离线模型输出")

## 借助离线模型查看软件基础版本号

### 场景介绍

不同CANN软件包版本，由于软件功能差异，所转换出的离线模型功能也有差异，该场景下建议用户使用匹配CANN软件版本的ATC工具重新进行模型转换。假如用户已有转换好的离线模型，想查看使用的CANN软件包基础版本号，则可以参见本章节完成。

### 查看方法

1. 获取已经转换好的离线模型，例如_tf\_resnet50.om_，并以CANN软件包运行用户将其上传至开发环境任意目录，例如上传到$HOME_/module__/_目录下。
2. 将离线模型转成JSON文件：

    ```bash
    atc --mode=1 --om=$HOME/module/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json
    ```

    - --om：指定**离线模型文件**_tf\_resnet50.om_所在路径。
    - --json：转换为JSON格式的文件路径和文件名。

    在转换后的JSON文件中，可以查看原始模型转换为离线模型时，使用的基础版本号，示例如下（如下为部分JSON片段），_<version\>_即为展示的版本号信息：

    ```json
       {
          "key": "opp_version",
          "value": {
            "s": "<version>"
          }
        },
        ... ...
        {
          "key": "atc_version",
          "value": {
            "s": "<version>"
          }
        },
        ... ...
        {
           "key": "atc_cmdline",
           "value": {
             "s": "xxx/atc.bin --model ./resnet50_tensorflow.pb  --framework 3 --output ./out/tf_resnet50 --soc_version <soc_version>"
           }
         },
        ... ...
        {
          "key": "soc_version",
          "value": {
            "s": "<soc_version>"
           }
         },
    ```
