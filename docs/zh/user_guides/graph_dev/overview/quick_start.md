# 快速入门

## 使用ATC命令转换ONNX模型

本节介绍如何使用ATC命令转换.onnx格式的模型文件，并在AI处理器上执行推理。

对于PyTorch、TensorFlow、MindSpore等前端框架导出的模型文件，GE提供了模型转换的能力，支持通过ATC命令行工具将模型转化为适配AI处理器的离线模型，并基于图模式执行推理。ATC目前已支持：

- \*.onnx格式的模型转换成\*.om格式的模型
- \*.pb格式的模型转换成\*.om格式的模型
- \*.air格式的模型转换成\*.om格式的模型
- \*.prototxt与\*.caffemodel格式的模型转换成\*.om格式的模型

下面以\*.onnx格式的模型转换成\*.om格式的模型为例，介绍基本使用方法：

1. 获取ONNX网络模型。

    单击[Link](https://github.com/onnx/models/blob/main/Computer_Vision/resnet50_Opset16_timm/resnet50_Opset16.onnx)下载该文件，再以CANN软件包运行用户将获取的文件上传至开发环境任意目录，例如上传到$HOME_/module__/_目录下。

2. 使用ATC命令行工具进行模型转换（以\*.onnx模型举例）。

    ```bash

    atc --model=$HOME/module/resnet50.onnx --framework=5 --output=$HOME/module/out/onnx_resnet50 --soc_version=<soc_version>
    ```

    - --model：原始网络模型文件路径与文件名。
    - --framework：原始网络模型框架类型，“3”代表TensorFlow模型，“5”代表ONNX模型，“1”代表MindSpore框架\*.air格式的模型文件或TorchAir通过export导出的标准\*.air格式文件
    - --output：存放转换后的离线模型的路径以及文件名，例如，若配置为“_$HOME__/module__/out/tf\_resnet50_”，则转换后的离线模型存储路径为“_$HOME__/module__/out/_”，转换后的离线模型名称为“_tf\_resnet50_.om”。
    <!-- npu="950,A3,910b,910,310p,310b" id1 -->
    - --soc\_version：AI处理器的型号。取值查询方法如下：
        <!-- npu="910b,910,310p,310b" id3 -->
        - 针对如下产品：在安装AI处理器的服务器执行`npu-smi info`命令进行查询，获取**Name**信息。实际配置值为AscendName，例如**Name**取值为`xxxyy`，实际配置值为`Ascendxxxyy`。

            Atlas A2 训练系列产品/Atlas A2 推理系列产品

            Atlas 200I/500 A2 推理产品

            Atlas 推理系列产品

            Atlas 训练系列产品
        <!-- end id3 -->
        <!-- npu="A3" id2 -->
        - 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行`npu-smi info -t board -i id -c chip_id`命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为`Ascendxxx`，**NPU Name**取值为1234，实际配置值为`Ascendxxx_1234`。其中：
            - id：设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
            - chip\_id：芯片id，通过**npu-smi info -m**命令查出的Chip ID即为芯片id。
        <!-- end id2 -->
        <!-- npu="950" id4 -->
        - 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行`npu-smi info -t board -i id`命令进行查询，获取**Chip Name**和**NPU Name**信息，实际配置值为Chip Name\_NPU Name。例如**Chip Name**取值为`Ascendxxx`，**NPU Name**取值为1234，实际配置值为`Ascendxxx_1234`。

            其中，id为设备id，通过**npu-smi info -l**命令查出的NPU ID即为设备id。
        <!-- end id4 -->
    <!-- end id1 -->

    <!-- @ref: ge/res/docs/zh/user_guides/graph_dev/quick_start_res.md#id1 -->

    更多参数以及详细使用方法请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。

3.若提示如下信息，则说明模型转换成功。

   ```bash
   ATC run success, welcome to the next use.
   ```

   成功执行命令后，\*.onnx模型将会被转换成适配AI处理器的\*.om离线模型，在--output参数指定的路径下，可查看生成的\*.om离线模型文件。

4.后续操作。

   转换成\*.om离线模型后，可以调用模型加载接口加载转换好的\*.om离线模型，再调用模型执行接口进行推理。如下为从文件加载模型文件并执行推理的简要流程，详细的模型加载与推理的方法可参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”章节。

  ```c++
  // 1.指定模型路径
  const char* omModelPath = "./model/onnx_resnet50.om";
  uint32_t modelId;
  // 2.加载离线模型文件，模型加载成功，返回标识模型的ID
  ret = aclmdlLoadFromFile(omModelPath, &modelId);
  // 3.执行模型，inputDs和outputDs为模型输入输出的Dataset对象
  ret = aclmdlExecute(modelId, inputDs, outputDs);
  // 4.卸载模型
  ret = aclmdlUnload(modelId);
  ```

## 使用接口全新构建Graph

GE的C++图引擎接口提供了两种构图方式：

- **使用图引擎接口全新构建Graph**：用户通过图引擎接口，逐个将算子添加到计算图中，构建成Ascend IR表示的计算图。
- **使用Parser接口将原始模型解析为Graph**：用户通过Parser接口，将框架模型文件（如\*.onnx和\*.pb等格式）解析成Ascend IR表示的计算图。

本章节综合上述两种场景，为您介绍如何构建一个Graph，构建完Graph后，可以根据自身需要修改Graph，然后将该Graph编译为适配AI处理器的离线模型。整体流程如下：

![](../figures/overall_process.png)

流程说明如下：

1. 进行构建Graph操作之前，先进行环境搭建，安装相应的CANN软件包，并进行网络结构分析；如果有不支持的算子，则开发对应算子，并部署到硬件环境。
    1. 如果**使用图引擎接口全新构建Graph**，需要根据原始网络，明确如下信息：
        1. 网络中包含哪些算子，以及这些算子的输入、输出、属性等信息。
        2. 网络中算子之间的关联关系。
        3. 确认原始网络中的算子在AI处理器是否支持，当前支持的算子请参见[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist)中的“Ascend IR算子规格说明”章节。如果不支持或不满足实际需要，开发者可以[《Ascend C算子开发指南》](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)自定义Ascend C算子，或者参见[《TBE&AI CPU算子开发》](https://hiascend.com/document/redirect/CannCommunityOpdevWizard)自定义TBE算子，选定一种方式后，将算子部署到硬件环境即可。

    2. 如果**使用Parser接口将原始模型解析为Graph**，需要确认原始网络中的算子在AI处理器是否支持，当前支持的算子请参见[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist)中的“Ascend IR算子规格说明”章节。如果不支持或不满足实际需要，可参见[《Ascend C算子开发指南》](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)自定义Ascend C算子，或者参见[《TBE&AI CPU算子开发》](https://hiascend.com/document/redirect/CannCommunityOpdevWizard)自定义TBE算子，选定一种方式后，将算子部署至硬件环境即可。

2. 构建Graph，开发者可以使用图引擎接口全新构建Graph，也可以使用Parser接口将原始模型解析为Graph。
3. 修改Graph，如果开发者想要优化Graph结构，则可以基于构建好的Graph直接将Graph修改为期望的结构。
4. 编译和运行Graph，开发者可以将修改后的Graph编译成适配AI处理器的离线模型，然后通过模型加载接口加载后上板推理；也可以直接运行Graph，得到Graph的运行结果。

### 获取样例

单击[使用接口构建Graph](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/IRBuild)获取样例代码，目录结构如下：

```text
├── src
│   ├──main.cpp                // 实现文件
├── Makefile                    // 编译脚本
├── CMakeLists.txt              // 编译脚本
├── data
│   ├──data_generate.py        // 使用图引擎接口全新构建Graph时，用于生成Graph所需要的数据信息，例如权重、偏置等数据
│   ├──tensorflow_generate.py  // 使用Parser接口将TensorFlow原始模型解析为Graph时，用于生成.pb格式的TensorFlow模型
│   ├──caffe_generate.py       // 使用Parser接口将Caffe原始模型解析为Graph时，用于生成.pbtxt格式的Caffe模型与.caffemodel格式的权重文件
├── scripts
│   ├──host_version.conf       // version配置
│   ├──testcase_300.sh         // 测试脚本
```

该样例主要包括如下功能：

1. 构建Graph。
    - **使用图引擎接口全新构建Graph**

        本样例构造的Graph结构如下：

        ![Graph结构](../figures/ir_build_image1.png)

    - **使用Parser接口将原始模型解析为Graph**，此处以TensorFlow为例进行介绍。

        本样例给出的TensorFlow原始模型结构为：

        ![TensorFlow原始模型结构](../figures/ir_build_image2.png)

        解析后的Graph结构为：

        ![解析后的Graph结构](../figures/ir_build_image3.png)

2. 修改Graph：此处以使用Parser接口将TensorFlow模型解析得到的Graph为例进行说明。

    修改后的Graph结构如下，在Add算子和Const算子之间插入Abs算子：

    ![插入Abs算子](../figures/ir_build_image4.png)

3. 编译Graph：将上述最终的Graph编译为离线模型。

### 编译运行

1. 准备环境。
    - 参见[环境准备](environment_setup.md)搭建环境，并配置环境变量。
    - 如果将TensorFlow框架原始模型解析为Graph，需要搭建TensorFlow1.15.0环境，如下为root用户在x86架构安装示例：

        ```python
        pip3 install tensorflow-cpu==1.15
        ```

        如果为非root用户安装，需要在上述命令末尾增加--user参数；如果为aarch64架构，安装方法请参见[《TensorFlow 1.15模型迁移》](https://gitcode.com/cann/tensorflow/blob/master/docs/zh/tfadapter_1/installation/tensorflow-1-15_install.md) \> 安装开源框架TensorFlow 1.15。

2. 准备构图数据。
    - **使用图引擎接口全新构建Graph**。

        在data目录执行数据生成脚本，命令如下：

        ```python
        python3 data_generate.py
        ```

        执行完成后，在data目录下生成.bin格式的数据，后续模型构建时会从该文件读取权重、偏置等数据。

    - **使用Parser接口将原始模型解析为Graph**。

        在data目录执行原始模型生成脚本，命令如下：

        ```python
        python3 tensorflow_generate.py
        ```

        执行完成后，在data目录下生成.pb格式的模型文件，后续将原始模型解析时会使用该模型，名称为tf\_test.pb。

3. 编译代码。
    1. （可选）打开Makefile文件，修改如下**ASCEND\_PATH**路径，修改为CANN软件安装目录/cann，例如root用户修改为：/usr/local/Ascend/cann。

        ```makefile
        ifeq (${ASCEND_INSTALL_PATH},)
             ASCEND_PATH := /usr/local/Ascend/cann
        ```

    2. 执行编译。

        在Makefile文件同级目录执行如下命令进行编译：

        ```bash
        make clean && make ir_build
        ```

        编译完成后，在当前路径out文件夹下生成可执行文件**ir\_build**。

4. 运行代码。

    - **使用图引擎接口全新构建Graph**。

        在out文件夹下执行如下命令：

        ```bash
        ./ir_build <soc_version> gen
        ```

        若提示如下信息，则说明运行成功，在同级目录生成\*.om离线模型文件。

        ```text
        ========== Generate Graph1 Success!==========
        Build Model1 SUCCESS!
        Save Offline Model1 SUCCESS!
        ```

    - **使用Parser接口将原始模型解析为Graph**。

        在out文件夹下执行如下命令：

        ```bash
        ./ir_build <soc_version> tf
        ```

        <soc\_version\>取值查询方法请参见[使用ATC命令转换ONNX模型](#使用atc命令转换onnx模型)。
        若提示如下信息，则说明运行成功，在同级目录生成\*.om离线模型文件。

        ```text
        ========== Generate graph from tensorflow origin model success.==========
        Modify Graph Start.
        Find src node: const.
        Find dst node: add.
        Modify Graph Success.
        ========== Modify tensorflow origin graph success.==========
        Build Model1 SUCCESS!
        Save Offline Model1 SUCCESS!
        ```

5. 后续操作。

    转换成\*.om离线模型后，可以调用模型加载接口加载转换好的\*.om离线模型，再调用模型执行接口进行推理。详细的模型加载与推理的方法可参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”。

### 代码解析

体验快速入门样例的编译运行后，若还想了解代码的实现逻辑，可查看下文的代码解析。此处按各函数的功能来介绍代码逻辑，为了便于阅读、理解代码，将各函数相关的变量与函数放在一起介绍。

打开“src/main.cpp”文件，先从main函数开始，了解整个样例的代码串接逻辑，再分别了解各自定义函数的实现。

1. main函数用来串联整个构建Graph的代码逻辑。

    ```c++
    int main(int argc, char* argv[])
    {
        // 1.构建Graph
        Graph graph1("IrGraph1");
           // 1.1使用图引擎接口全新构建Graph
            ret = GenGraph(graph1);

           // 1.2使用Parser接口将原始模型解析为Graph
            std::string tfPath = "../data/tf_test.pb";
            std::map<AscendString, AscendString> parser_options;
            tfStatus = ge::aclgrphParseTensorFlow(tfPath.c_str(), parser_options, graph1);

           // 2.修改Graph：此处以从TensorFlow模型解析得到的Graph为例进行修改
            ModifyGraph(graph1);

        // 3.系统初始化，并申请资源
        std::map<AscendString, AscendString> global_options = {
            {AscendString(ge::ir_option::SOC_VERSION), AscendString(argv[kSocVersion])}  ,
        };
        auto status = aclgrphBuildInitialize(global_options);

        // 4.编译Graph
        ModelBufferData model1;
        std::map<AscendString, AscendString> options;
        PrepareOptions(options);
        status = aclgrphBuildModel(graph1, options, model1);

        // 5.将内存缓冲区中的模型保存为离线模型文件
        status = aclgrphSaveModel("ir_build_sample1", model1);

        // 6.释放资源
        aclgrphBuildFinalize();

    }
    ```

2. include依赖的头文件。

    ```c++
    #include <string.h>
    #include "tensorflow_parser.h"
    #include "graph.h"
    #include "types.h"
    #include "tensor.h"
    #include "attr_value.h"
    #include "ge_error_codes.h"
    #include "ge_api_types.h"
    #include "ge_ir_build.h"
    #include "all_ops.h"
    ... ...
    using namespace std;
    using namespace ge;
    using ge::Operator;
    ```

3. 构建Graph。

    使用算子原型衍生接口定义算子，并连接算子。详细代码示例为：

    ```c++
    bool GenGraph(Graph& graph)
    {
        auto shape_data = vector<int64_t>({ 1,1,28,28 });
        TensorDesc desc_data(ge::Shape(shape_data), FORMAT_ND, DT_FLOAT16);
        // 创建Data算子实例
        auto data = op::Data("data");
        data.update_input_desc_x(desc_data);
        data.update_output_desc_y(desc_data);

        // 创建Add算子实例，并与Data算子连接
        auto add = op::Add("add")
            .set_input_x1(data)
            .set_input_x2(data);

        // 创建AscendQuant算子实例，并与Data算子连接
        auto quant = op::AscendQuant("quant")
            .set_input_x(data)
            .set_attr_scale(1.0)
            .set_attr_offset(0.0);

        auto conv_weight = op::Const("Conv2D/weight")
            .set_attr_value(weight_tensor);
        // 创建Conv2D算子实例，并与AscendQuant算子连接
        auto conv2d = op::Conv2D("Conv2d1")
            .set_input_x(quant)
            .set_input_filter(conv_weight)
            .set_attr_strides({ 1, 1, 1, 1 })
            .set_attr_pads({ 0, 1, 0, 1 })
            .set_attr_dilations({ 1, 1, 1, 1 });
            ... ...

        auto dequant_scale = op::Const("dequant_scale")
            .set_attr_value(dequant_tensor);
        // 创建AscendDequant算子实例，并与Conv2D算子连接
        auto dequant = op::AscendDequant("dequant")
            .set_input_x(conv2d)
            .set_input_deq_scale(dequant_scale);
            ... ...

        auto bias_weight_1 = op::Const("Bias/weight_1")
            .set_attr_value(weight_bias_add_tensor_1);
        // 创建BiasAdd算子实例，并与AscendDequant算子连接
        auto bias_add_1 = op::BiasAdd("bias_add_1")
            .set_input_x(dequant)
            .set_input_bias(bias_weight_1)
            .set_attr_data_format("NCHW");
            ... ...

        auto dynamic_const = op::Const("dynamic_const").set_attr_value(dynamic_const_tensor);
        // 创建Reshape算子实例，并与BiasAdd算子连接
        auto reshape = op::Reshape("Reshape")
            .set_input_x(bias_add_1)
            .set_input_shape(dynamic_const);
            ... ...

        auto matmul_weight_1 = op::Const("dense/kernel")
            .set_attr_value(matmul_weight_tensor_1);
        // 创建MatMul算子实例，并与Reshape算子连接
        auto matmul_1 = op::MatMul("MatMul_1")
            .set_input_x1(reshape)
            .set_input_x2(matmul_weight_1);
            ... ...

        auto bias_add_const_1 = op::Const("dense/bias")
            .set_attr_value(bias_add_const_tensor_1);
        // 创建BiasAdd算子实例，并与MatMul算子连接
        auto bias_add_2 = op::BiasAdd("bias_add_2")
            .set_input_x(matmul_1)
            .set_input_bias(bias_add_const_1)
            .set_attr_data_format("NCHW");
        // 创建Relu6算子实例，并与BiasAdd算子连接
        auto relu6 = op::Relu6("relu6")
            .set_input_x(bias_add_2);
            ... ...

        auto matmul_weight_2 = op::Const("OutputLayer/kernel")
            .set_attr_value(matmul_weight_tensor_2);
        // 创建MatMul算子实例，并与Relu6算子连接
        auto matmul_2 = op::MatMul("MatMul_2")
            .set_input_x1(relu6)
            .set_input_x2(matmul_weight_2);
            ... ...

        auto bias_add_const_3 = op::Const("OutputLayer/bias")
            .set_attr_value(bias_add_const_tensor_3);
        // 创建BiasAdd算子实例，并与MatMul算子连接
        auto bias_add_3 = op::BiasAdd("bias_add_3")
            .set_input_x_by_name(matmul_2, "y")
            .set_input_bias_by_name(bias_add_const_3, "y")
            .set_attr_data_format("NCHW");
        // 创建SoftmaxV2算子实例，并与BiasAdd算子连接
        auto softmax = op::SoftmaxV2("Softmax")
            .set_input_x_by_name(bias_add_3, "y");
        std::vector<Operator> inputs{ data };

        std::vector<Operator> outputs{ softmax, add };
        std::vector<std::pair<ge::Operator, std::string>> outputs_with_name = {{softmax, "y"}};
        // 调用接口，设置Graph的输入输出
        graph.SetInputs(inputs).SetOutputs(outputs);
        return true;
    }
    ```

4. 修改Graph：此处以从TensorFlow模型解析得到的Graph为例进行修改。

    在Add算子和Const算子之间插入Abs算子：

    ```c++
    bool ModifyGraph(Graph &graph) {

        const std::string CONST = "const1";
        const std::string ADD = "input3_add";
        GNode src_node;
        GNode dst_node;
        // 获取图中的所有节点
        std::vector<GNode> nodes = graph.GetAllNodes();
        graphStatus ret = GRAPH_FAILED;
        for (auto &node : nodes) {
            ge::AscendString name;
            // 获取图中算子的名称
            ret = node.GetName(name);

            std::string node_name(name.GetString());
        }
        // 删除图中的指定连接边
        ret = graph.RemoveEdge(src_node, 0, dst_node, 1);

        auto abs = op::Abs("input3_abs");

        // 新增abs节点到图上
        GNode node_abs = graph.AddNodeByOp(abs);

       //新增数据边，连接新增的节点
        ret = graph.AddDataEdge(src_node, 0, node_abs, 0);

        ret = graph.AddDataEdge(node_abs, 0, dst_node, 1);
        return true;
    }
    ```

5. 编译Graph。

    构建和修改完Graph之后，调用接口进行Graph编译操作，接口调用流程如下图所示，详细流程请参见main\(\)函数。

    ![构建graph流程](../figures/ir_graph_construction.png)
