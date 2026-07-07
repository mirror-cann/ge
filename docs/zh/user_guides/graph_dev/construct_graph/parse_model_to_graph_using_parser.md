# 使用Parser接口将原始模型解析为Graph

除了可以使用算子原型直接构图外，CANN还提供了框架解析功能，将主流框架的模型格式解析成CANN模型格式。

## 功能介绍

目前业界开源的深度学习框架（例如TensorFlow，PyTorch、Caffe等），定义模型的格式各有不同，例如TensorFlow通过自定义pb描述静态shape图和模型，PyTorch通过ONNX规范描述，因此需要通过统一的框架解析功能隔离上层框架差异，通过Parser模块完成解析并转换成AI处理器支持的CANN模型格式。

![图示](../figures/feature_intro_0.png)

涉及的主要接口为：

- 解析TensorFlow模型：[aclgrphParseTensorFlow](../../../api/graph_engine_api/cpp/ge/aclgrphParseTensorFlow.md)
- 解析Caffe模型：[aclgrphParseCaffe](../../../api/graph_engine_api/cpp/ge/aclgrphParseCaffe.md)

    <!-- npu="910b" id1 -->
    Atlas A2 训练系列产品/Atlas A2 推理系列产品不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
    <!-- end id1 -->

    <!-- npu="A3" id2 -->
    Atlas A3 训练系列产品/Atlas A3 推理系列产品不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
    <!-- end id2 -->

    <!-- npu="950" id3 -->
    Ascend 950PR/Ascend 950DT不支持Caffe框架：Caffe框架在该产品形态已不演进，不保证功能可用。
    <!-- end id3 -->

    <!-- @ref: ge/res/docs/zh/user_guides/graph_dev/parse_model_to_graph_using_parser_res.md#id1 -->

- 解析ONNX原始模型：[aclgrphParseONNX](../../../api/graph_engine_api/cpp/ge/aclgrphParseONNX.md)
- 解析加载至内存的ONNX模型：[aclgrphParseONNXFromMem](../../../api/graph_engine_api/cpp/ge/aclgrphParseONNXFromMem.md)

Parser层目前为用户开放了自定义OpParser和自定义TensorFlow Scope融合规则的功能，如果用户在Parser解析时需要对框架进行更灵活的适配，则可以自定义OpParser或自定义开发TensorFlow Scope融合规则。

- 自定义OpParser：

  如果用户需要将原始框架中算子直接映射到CANN中已实现的Ascend C算子或者已实现的TBE算子，可直接进行第三方框架的适配，需分别参见[《Ascend C算子开发指南》](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)中的“编程指南 \> 附录 \> AI框架算子适配”章节或者参见[《TBE&AI CPU算子开发》](https://hiascend.com/document/redirect/CannCommunityOpdevWizard)中的“算子开发过程 \> 算子适配”章节，选择其中一种已实现的算子即可。

- 自定义TensorFlow Scope融合规则：基于TensorFlow构建的神经网络计算图通常由大量的小算子组成，为了实现高性能的计算，往往需要对子图中的小算子进行融合，使得融合后的大算子可以充分利用硬件加速资源。具体请参见《TensorFlow Parser Scope融合规则开发》。

>[!NOTE]说明
> <!-- npu="950" id4 -->
>该说明**不适用于**Ascend 950PR/Ascend 950DT：
> <!-- end id4 -->
>原始模型转换为Graph时，如果Tensor的shape维度和format维度数量不一致，按照如下表格中的规则理解当前维度：
>例如，shape只有1维为\[16\]，format为4维，比如NHWC，该场景下可以理解为shape的1维为C轴，其他轴需要补维，补维后格式为\[1,1,1,16\]；
>shape为2维\[16,16\]，format为4维，比如NHWC，该场景下可以理解为shape的2维为HW轴，其他轴需要补维，补维后格式为\[1,16,16,1\]。

|实际维度数|format|维度理解为|
|--|--|--|
|1|NCHWNHWCHWCNCHWNNDHWCNCDHWDHWCNDHWNC|C|
|2|NCHW|CH|
|2|NHWC|HW|
|2|HWCN|CN|
|2|CHWN|WN|
|2|NDHWC|WC|
|2|NCDHW|HW|
|2|DHWCN|CN|
|2|DHWNC|NC|
|3|NCHW|CHW|
|3|NHWC|HWC|
|3|HWCN|WCN|
|3|CHWN|HWN|
|3|NDHWC|HWC|
|3|NCDHW|DHW|
|3|DHWCN|WCN|
|3|DHWNC|WNC|
|4|NDHWC|DHWC|
|4|NCDHW|CDHW|
|4|DHWCN|HWCN|
|4|DHWNC|HWNC|

## 基于TensorFlow模型解析

包含的头文件：

```c++
#include "tensorflow_parser.h"
```

通过[aclgrphParseTensorFlow](../../../api/graph_engine_api/cpp/ge/aclgrphParseTensorFlow.md)接口将TensorFlow原始模型解析为Graph，此时Graph保存在内存缓冲区中。

```c++
std::string tfPath = "../data/tf_test.pb";
ge::Graph graph1;
auto tfStatus = ge::aclgrphParseTensorFlow(tfPath.c_str(),graph1);
```

同时，支持用户指定**parser\_params**：

```c++
std::string tfPath = "../data/tf_test.pb";
std::map<ge::AscendString, ge::AscendString> parser_params = {
            {ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
            {ge::AscendString(ge::ir_option::OUTPUT), ge::AscendString("newIssue")}};
ge::Graph graph1;
auto tfStatus = ge::aclgrphParseTensorFlow(tfPath.c_str(), parser_params, graph1);
```

## 基于Caffe模型解析

包含的头文件：

```c++
#include "caffe_parser.h"
```

通过[aclgrphParseCaffe](../../../api/graph_engine_api/cpp/ge/aclgrphParseCaffe.md)接口将Caffe原始模型解析为Graph，此时Graph保存在内存缓冲区中。

```c++
std::string caffePath = "../data/caffe_test.prototxt";
std::string weight = "../data/caffe_test.caffemodel";
ge::Graph graph1;
auto caffeStatus = ge::aclgrphParseCaffe(caffePath.c_str(), weight.c_str(), graph1);
```

同时，支持用户指定**parser\_params：**

```c++
std::string caffePath = "../data/caffe_test.prototxt";
std::string weight = "../data/caffe_test.caffemodel";
std::map<ge::AscendString, ge::AscendString> parser_params = {
            {ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
            {ge::AscendString(ge::ir_option::OUTPUT), ge::AscendString("newIssue")}};
ge::Graph graph1;
auto caffeStatus = ge::aclgrphParseCaffe(caffePath.c_str(), weight.c_str(), parser_params, graph1);
```

## 基于ONNX模型解析

包含的头文件：

```c++
#include "onnx_parser.h"
```

通过[aclgrphParseONNXFromMem](../../../api/graph_engine_api/cpp/ge/aclgrphParseONNXFromMem.md)接口将加载至内存的ONNX模型解析为Graph，此时Graph保存在内存缓冲区中。同时，支持用户指定**parser\_params：**

```c++
// 以二进制方式打开一个ONNX模型文件
FILE *pFile = fopen("./onnx/resnet101.onnx", "rb" );
if(pFile==NULL)
{
    fputs("File error",stderr);
    exit(1);
}

// 读取文件大小
fseek(pFile, 0, SEEK_END);
long lSize = ftell(pFile);
rewind(pFile);

// 分配足够的内存缓冲区
char *buffer =(char*) malloc(sizeof(char)*lSize);
if(buffer == NULL)
{
    fputs("Memory error", stderr);
    exit(2);
}
// 把文件内容读入缓冲区
size_t result = fread(buffer, 1, lSize, pFile);
if(result != lSize)
{
    fputs("Reading error", stderr);
    exit(3);
}
// 准备ONNX解析时需要的参数
std::map<ge::AscendString, ge::AscendString> parser_params= {
            {ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
            {ge::AscendString(ge::ir_option::OUTPUT), ge::AscendString("newIssue")}};
// 解析ONNX模型
ge::Graph graph1;
auto onnxStatus = ge::aclgrphParseONNXFromMem(buffer, result, parser_params, graph1);
```

通过[aclgrphParseONNX](../../../api/graph_engine_api/cpp/ge/aclgrphParseONNX.md)接口将ONNX原始模型解析为Graph，此时Graph保存在内存缓冲区中。同时，支持用户指定**parser\_params：**

```c++
std::string onnxPath = "../data/onnx_test.onnx";
std::map<ge::AscendString, ge::AscendString> parser_params= {
            {ge::AscendString(ge::ir_option::INPUT_FP16_NODES), ge::AscendString("input1;input2")},
            {ge::AscendString(ge::ir_option::OUTPUT), ge::AscendString("newIssue")}};
ge::Graph graph1;
auto onnxStatus = ge::aclgrphParseONNX(onnxPath.c_str(), parser_params, graph1);
```
