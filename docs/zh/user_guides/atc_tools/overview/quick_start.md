# 快速入门

本章节以各框架下模型转换为例，演示如何快速转换一个离线模型。

> [!NOTE]说明
>
> - **版本兼容性说明：**
> - **针对8.5.0及之后版本**：使用ATC工具进行模型转换时，必须安装与目标AI处理器匹配的ops算子包，否则会导致编译失败。
> - 低版本的CANN软件包环境上转换出的离线模型，支持在高版本的CANN软件包环境上运行，兼容4个版本周期。
><!-- npu="950,A3,910b,910,310p,310b" id1 -->
> - 若开发环境架构为Arm（aarch64），模型转换耗时较长，则可以参考[开发环境架构为Arm（aarch64）时模型转换耗时较长](../FAQ/arm_aarch64_conversion_slow.md)解决。
><!-- end id1 -->
> - 如果模型转换时，用户使用了设置网络模型精度参数[--precision\_mode](../CLI_options/--precision_mode.md)或[--precision\_mode\_v2](../CLI_options/--precision_mode_v2.md)：
>   - 上述两个参数默认都为性能优先，后续推理时可能会导致精度溢出问题。如果推理时出现精度问题，可以参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理 \> 精度/性能优化 \> 模型推理精度提升建议”进行定位。
>   - 如果用户聚焦精度问题，可以修改为其他取值，比如--precision\_mode设置为must\_keep\_origin\_dtype或--precision\_mode\_v2设置为origin。

## 开源框架的TensorFlow网络模型转换成离线模型

1. 获取TensorFlow网络模型。

    单击[Link](https://gitee.com/ascend/ModelZoo-TensorFlow/tree/master/TensorFlow/contrib/cv/resnet50_for_TensorFlow)，根据页面提示获取ResNet50网络的模型文件（\*.pb），并以CANN软件包运行用户将获取的文件上传至开发环境任意目录，例如上传到$HOME/module/目录下。

2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow_1.7.pb --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version>
    ```

    - --model：ResNet50网络模型文件所在路径。
    - --framework：原始框架类型，3表示TensorFlow。
    - --output：生成的离线模型路径。
    - --soc\_version：AI处理器的型号。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)，请使用与芯片名相对应的_<soc\_version\>_取值进行模型转换，然后再进行推理，具体使用芯片查询方法请参见[--soc\_version](../CLI_options/--soc_version.md)。

3. 若提示如下信息，则说明模型转换成功。

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：tf\_resnet50.om）。
    <!-- npu="950,A3,910b,910,310p,310b" id2 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id2 -->

## ONNX网络模型转换成离线模型

1. 获取ONNX网络模型。

    单击[Link](https://gitcode.com/Ascend/ModelZoo-PyTorch/tree/master/ACL_PyTorch/built-in/cv/Resnet50_Pytorch_Infer)进入ModelZoo页面，查看README.md中“快速上手\>模型推理”章节获取\*.onnx模型文件，再以CANN软件包运行用户将获取的文件上传至开发环境任意目录，例如上传到$HOME/module/目录下。

2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_official.onnx --framework=5 --output=$HOME/module/out/onnx_resnet50 --soc_version=<soc_version>
    ```

    - --model：ResNet50网络模型文件所在路径。
    - --framework：原始框架类型，5表示ONNX。
    - --output：生成的离线模型路径。
    - --soc\_version：AI处理器的型号。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)，请使用与芯片名相对应的_<soc\_version\>_取值进行模型转换，然后再进行推理，具体使用芯片查询方法请参见[--soc\_version](../CLI_options/--soc_version.md)。

3. 若提示如下信息，则说明模型转换成功。

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：onnx\_resnet50.om）。
    <!-- npu="950,A3,910b,910,310p,310b" id3 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id3 -->

## 开源框架的Caffe网络模型转换成离线模型

1. 获取Caffe网络模型。

    您可以从以下链接中获取ResNet50网络的模型文件（\*.prototxt）、权重文件（\*.caffemodel），并以CANN软件包运行用户将获取的文件上传至开发环境任意目录，例如上传到$HOME/module/目录下。

    - ResNet50网络的模型文件（\*.prototxt）：单击[Link](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/resnet50/resnet50.prototxt)下载该文件。
    - ResNet50网络的权重文件（\*.caffemodel）：单击[Link](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com/003_Atc_Models/AE/ATC%20Model/resnet50/resnet50.caffemodel)下载该文件。

2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50.prototxt --weight=$HOME/module/resnet50.caffemodel --framework=0 --output=$HOME/module/out/caffe_resnet50 --soc_version=<soc_version>
    ```

    - --model：ResNet50网络模型文件所在路径。
    - --weight：ResNet50网络权重文件所在路径。
    - --framework：原始框架类型，0表示Caffe。
    - --output：生成的离线模型路径。
    - --soc\_version：AI处理器的型号。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)，请使用与芯片名相对应的_<soc\_version\>_取值进行模型转换，然后再进行推理，具体使用芯片查询方法请参见[--soc\_version](../CLI_options/--soc_version.md)。

3. 若提示如下信息，则说明模型转换成功。

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：caffe\_resnet50.om）。
    <!-- npu="950,A3,910b,910,310p,310b" id4 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id4 -->

## \*.air格式的模型文件转换成离线模型

1. 获取\*.air格式模型文件。

    单击[Link](https://obs-9be7.obs.cn-east-2.myhuaweicloud.com:443/cannInfo/model/resnet50_export.air)，获取ResNet50网络的模型文件，并以CANN软件包运行用户将获取的文件上传到开发环境任意路径，例如$HOME/module/目录下。

2. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --model=$HOME/module/resnet50_export.air --framework=1 --output=$HOME/module/out/resnet50_air --soc_version=<soc_version>
    ```

    - --model：\*.air格式的模型文件所在路径。
    - --framework：原始框架类型，1表示\*.air格式的模型文件。
    - --output：生成的离线模型路径。
    - --soc\_version：AI处理器的型号。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)，请使用与芯片名相对应的_<soc\_version\>_取值进行模型转换，然后再进行推理，具体使用芯片查询方法请参见[--soc\_version](../CLI_options/--soc_version.md)。

3. 若提示如下信息，则说明模型转换成功。

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在--output参数指定的路径下，可查看离线模型（如：_resnet50_\_air.om）。
    <!-- npu="950,A3,910b,910,310p,310b" id5 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>“错误码参考”章节进行辅助定位。
    <!-- end id5 -->
