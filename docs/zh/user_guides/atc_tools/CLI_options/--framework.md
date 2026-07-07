# --framework

## 产品支持情况

全量芯片支持

## 功能说明

原始网络模型框架类型。

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT**不支持Caffe**框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id1 -->

<!-- npu="A3" id2 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品**不支持Caffe**框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id2 -->

<!-- npu="910b" id3 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品**不支持Caffe**框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id3 -->

<!-- npu="IPV350" id4 -->
IPV350：**不支持Caffe**框架：Caffe框架在该产品形态已不演进，不保证功能可用。
<!-- end id4 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--framework_res.md#id1 -->

## 关联参数

无。

## 参数取值

**参数值：**

- 0：Caffe
- 1：MindSpore框架\*.air格式的模型文件或TorchAir通过export导出的标准\*.air格式文件
- 3：TensorFlow
- 5：ONNX

**参数值约束：**

- 当[--mode](--mode.md)为1时，该参数可选，可以指定Caffe、TensorFlow、ONNX原始模型转成JSON文件，不指定时默认为离线模型转JSON文件，如果指定时需要保证[--om](--om.md)模型和[--framework](--framework.md)类型对应一致，例如：

    ```bash
    --mode=1 --framework=0 --om=$HOME/module/resnet50.prototxt
    --mode=1 --framework=3 --om=$HOME/module/resnet50_tensorflow.pb
    --mode=1 --framework=5 --om=$HOME/module/resnet50.onnx
    ```

- 当[--mode](--mode.md)为0或3时，该参数必选，可以指定Caffe、TensorFlow、MindSpore或ONNX。
- 当取值为0时，即为Caffe框架网络模型，模型包括后缀为prototxt的模型文件和后缀为caffemodel的权重文件，并且此两个文件的op name和op type必须保持名称一致（包括大小写）。
- 当取值为3时，即为TensorFlow框架网络模型，只支持FrozenGraphDef格式，即尾缀为pb的模型文件，pb文件采用protobuf格式存储，网络模型和权重数据都存储在同一个文件中。
- 当取值为5时，即为ONNX格式网络模型，支持ai.onnx算子域中opset v11\~v18版本的算子；而PyTorch框架的pth模型，可以转化为ONNX格式的模型或者通过TorchAir export导出标准的\*.air格式文件，然后才能进行模型转换。
- 当取值为1，且为MindSpore框架网络模型时，请务必查看如下限制：
  - 模型转换时，仅支持后缀为\*.air的模型文件；
  - [--mode](--mode.md)只支持配置为0；
  - [--input\_format](--input_format.md)只支持配置为NCHW，配置其它值无效，但模型转换成功；
  - MindSpore框架下，使用[--input\_shape](--input_shape.md)、[--out\_nodes](--out_nodes.md)、[--is\_output\_adjust\_hw\_layout](--is_output_adjust_hw_layout.md)、[--input\_fp16\_nodes](--input_fp16_nodes.md)、[--is\_input\_adjust\_hw\_layout](--is_input_adjust_hw_layout.md)、[--op\_name\_map](--op_name_map.md)参数不生效，但模型转换成功；
  - 当模型大小超过2G时，在MindSpore框架中保存模型时会同时生成\*.air文件、weight文件夹及其中的权重文件，在模型转换时，需要将weight文件夹与\*.air文件存放在同级目录下，否则模型转换报错。

## 推荐配置及收益

无。

## 示例

- ONNX网络模型：

    ```bash
    atc --mode=0 --framework=5 --model=$HOME/module/resnet50.onnx --output=$HOME/module/out/onnx_resnet50 --soc_version=<soc_version>
    ```

- TensorFlow框架：

    ```bash
    atc --mode=0 --framework=3 --model=$HOME/module/resnet50_tensorflow.pb --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version>
    ```

- Caffe框架：

    ```bash
    atc --mode=0 --framework=0 --model=$HOME/module/resnet50.prototxt --weight=$HOME/module/resnet50.caffemodel  --output=$HOME/module/out/caffe_resnet50 --soc_version=<soc_version>
    ```

- \*.air格式的模型文件：

    ```bash
    atc --mode=0 --framework=1 --model=$HOME/module/ResNet50.air --output=$HOME/module/out/ResNet50_mindspore --soc_version=<soc_version>
    ```

## 使用约束

- 如果用户使用Faster RCNN、YOLOv3、YOLOv2、SSD等Caffe框架网络模型进行模型转换，由于此类网络中包含了一些原始Caffe框架中没有定义的算子结构，如ROIPooling、Normalize、PSROI Pooling和Upsample等。为了使AI处理器能支持这些网络，需要对原始的Caffe框架网络模型进行扩展，降低开发者开发自定义算子/开发后处理代码的工作量，详细扩展方法请参见[定制网络修改（Caffe）](../custom_network_modify_caffe/README.md)。
- 针对TensorFlow框架原始网络模型，如果存在控制流算子（比如Switch/Merge/LoopCond/Case/While等），该类网络模型不能直接使用ATC工具进行模型转换，需要先将控制流算子的网络模型转成函数类算子的网络模型，然后利用ATC工具转换成适配AI处理器的离线模型，详细转换方式请参见[定制网络修改（TensorFlow）](../custom_network_modify_tf/README.md)。
