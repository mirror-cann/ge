# 动态shape

## 动态batch\_size

本节介绍如何在模型构建时支持动态batch\_size功能。

<!-- npu="IPV350" id1 -->
**IPV350不支持动态batch\_size特性。**
<!-- end id1 -->

### 功能介绍

batch\_size即每次模型推理处理的图片数，对于每次推理图片数量固定的场景，处理图片数由数据shape的N值决定；对于每次推理图片数量不固定的场景，则可以通过动态batch\_size功能来动态分配每次处理的图片数量。例如用户执行推理业务时需要每次处理2张、4张、8张图片，则可以在模型中配置档位信息2,4,8，申请了档位后，模型推理时会根据实际档位申请内存。

### 使用方法

1. 在Data算子定义时，将数据shape的指定维度设置为-1：

    ```c++
    auto shape_data = vector<int64_t>({ -1,1,28,28 });
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_NCHW, DT_FLOAT);
    auto data = op::Data("data");
    data.update_input_desc_data(desc_data);
    data.update_output_desc_out(desc_data);
    ```

2. 模型编译时，在aclgrphBuildModel接口options中设置INPUT\_SHAPE/INPUT\_FORMAT信息，同时通过DYNAMIC\_BATCH\_SIZE指定每个档位值的大小。

    > [!NOTE]说明
    >
    >- INPUT\_FORMAT必须设置并且和所有Data算子的format保持一致，且仅支持NCHW和NHWC，否则会导致模型编译失败。
    >- INPUT\_SHAPE参数必须设置。

    ```c++
    void PrepareOptions(std::map<AscendString, AscendString>& options) {
        options.insert({
            {ge::ir_option::INPUT_FORMAT, "NCHW"},
            {ge::ir_option::INPUT_SHAPE, "data:-1,1,28,28"}, // INPUT_SHAPE中的-1表示设置动态batch。
            {ge::ir_option::DYNAMIC_BATCH_SIZE, "2,4,8"}     // 设置N的档位
        });
    }
    ```

### 使用注意事项

- 该功能不能和动态分辨率、动态维度同时使用。

- 如果用户设置的档位数值过大或档位过多，可能会导致模型编译失败，此时建议用户减少档位或调低档位数值。
- 若用户执行推理业务时，每次处理的图片数量不固定，则可以通过配置该参数来动态分配每次处理的图片数量。例如用户执行推理业务时需要每次处理2张、4张、8张图片，则可以配置为2,4,8，申请了档位后，模型推理时会根据实际档位申请内存。
- 如果模型编译时通过该参数设置了动态batch\_size，则使用应用工程进行模型推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetDynamicBatchSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicBatchSize.md)接口，用于设置真实的batch\_size档位。
  - 不使用[aclmdlSetDynamicBatchSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicBatchSize.md)接口，则模型执行时，默认按照batch\_size设置范围的最大值进行赋值。

- 如果用户设置了动态batch\_size，同时又通过**INSERT\_OP\_FILE**参数设置了动态AIPP功能：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口设置动态AIPP相关参数值时，需确保batch\_size要设置为最大Batch数。

- 某些场景下，通过该参数设置动态batch\_size特性后，生成的离线模型网络结构会与固定batch\_size场景下的不同，推理性能可能存在差异。
- 如果用户设置的档位数值过大或档位过多，在运行环境执行推理时，建议执行**swapoff -a**命令关闭swap交换空间作为内存的功能，防止出现由于内存不足，将swap交换空间作为内存继续调用，导致运行环境异常缓慢的情况。

## 动态分辨率

本节介绍如何在模型构建时支持动态分辨率的功能。

<!-- npu="IPV350" id2 -->
**IPV350不支持动态分辨率特性。**
<!-- end id2 -->

### 功能介绍

在模型推理时，对于每次处理图片宽高不固定的场景，用户可以在模型构建时设置不同的图片宽高档位。

### 使用方法

1. 在Data算子定义时，将数据shape的HW维度都设置为-1：

    ```c++
    auto shape_data = vector<int64_t>({ 8,3,-1,-1 });
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_NCHW, DT_FLOAT);
    auto data = op::Data("data");
    data.update_input_desc_data(desc_data);
    data.update_output_desc_out(desc_data);
    ```

2. 模型编译时，在aclgrphBuildModel接口options中设置INPUT\_SHAPE/INPUT\_FORMAT信息，同时通过DYNAMIC\_IMAGE\_SIZE指定动态分辨率的档位。

    > [!NOTE]说明
    >
    >- INPUT\_FORMAT必须设置并且和所有Data算子的format保持一致，且仅支持NCHW和NHWC，否则会导致模型编译失败。
    >- INPUT\_SHAPE参数必须设置。

    ```c++
    void PrepareOptions(std::map<AscendString, AscendString>& options) {
        options.insert({
            {ge::ir_option::INPUT_FORMAT, "NCHW"},
            {ge::ir_option::INPUT_SHAPE, "data: 8,3,-1,-1"},
            {ge::ir_option::DYNAMIC_IMAGE_SIZE, "416,416;832,832"}  // 设置HW档位，支持处理HW为416,416，或者832,832的图片
        });
    }
    ```

### 使用注意事项

- 该功能不能和动态Batch、动态维度同时使用。
- 如果用户设置的分辨率数值过大或档位过多，可能会导致模型编译失败，此时建议用户减少档位或调低档位数值。
- 如果模型编译时通过该参数设置了动态分辨率，则使用应用工程进行模型推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口，用于设置真实的分辨率，且实际推理时，使用的数据集图片大小需要与具体使用的分辨率相匹配。
  - 不使用[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口，则模型执行时，默认按照动态分辨率设置范围的最大档位宽、高进行赋值。

- 如果用户设置了动态分辨率，即输入图片的宽和高不确定，同时又通过**INSERT\_OP\_FILE**参数设置了静态AIPP功能：该场景下，AIPP配置文件中不能开启Crop和Padding功能，并且需要将配置文件中的src\_image\_size\_w和src\_image\_size\_h取值设置为0。
- 如果用户设置了动态分辨率，同时又通过**INSERT\_OP\_FILE**参数设置了动态AIPP功能：

    实际推理时，调用[aclmdlSetInputAIPP](../../../api/graph_engine_api/c/acl/aclmdlSetInputAIPP.md)接口，设置动态AIPP相关参数值时，不能开启Crop和Padding功能。该场景下，还需要确保通过aclmdlSetInputAIPP接口设置的宽和高与[aclmdlSetDynamicHWSize](../../../api/graph_engine_api/c/acl/aclmdlSetDynamicHWSize.md)接口设置的宽、高相等，都必须设置成动态分辨率最大档位的宽、高。

- 某些场景下，通过该参数设置动态分辨率特性后，生成的离线模型网络结构会与固定分辨率场景下的不同，推理性能可能存在差异。
- 如果用户设置了动态分辨率，实际推理时，使用的数据集图片大小需要与具体使用的分辨率相匹配。
- 如果用户设置的分辨率数值过大或档位过多，在运行环境执行推理时，建议执行**swapoff -a**命令关闭swap交换空间作为内存的功能，防止出现由于内存不足，将swap交换空间作为内存继续调用，导致运行环境异常缓慢的情况。

## 动态维度

### 编译Graph为离线模型场景

当前系统支持在接口中配置动态维度信息，从而支持动态输入的场景。本节将给出详细说明。

#### 功能介绍

用户可以在模型构建时，设置ND格式下动态维度的档位。适用于执行推理时，每次处理任意维度的场景。

**IPV350不支持动态维度特性。**

#### 使用方法

1. 在Data算子定义时，将数据shape的动态维度设置为-1：

    ```c++
    auto shape_data = vector<int64_t>({ 1,-1,-1 });
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_ND, DT_FLOAT);
    auto data = op::Data("data");
    data.update_input_desc_data(desc_data);
    data.update_output_desc_out(desc_data);
    ```

2. 模型编译时，在aclgrphBuildModel接口options中设置INPUT\_SHAPE/INPUT\_FORMAT信息，同时通过DYNAMIC\_DIMS指定档位信息。

    > [!NOTE]说明
    >
    >- INPUT\_FORMAT必须设置并且和所有Data算子的format保持一致，且仅支持ND，否则会导致模型编译失败。
    >- INPUT\_SHAPE参数必须设置。

    ```c++
    void PrepareOptions(std::map<std::string, std::string>& options) {
        options.insert({
            {ge::ir_option::INPUT_FORMAT, "ND"},
            {ge::ir_option::INPUT_SHAPE, "data:1,-1,-1"},
            {ge::ir_option::DYNAMIC_DIMS, "1,2;3,4;5,6;7,8"}  // 模型编译时，支持的data算子的shape为1,1,2; 1,3,4; 1,5,6; 1,7,8
        });
    }
    ```

#### 使用注意事项

- 该功能不能和动态batch\_size、动态分辨率、AIPP功能同时使用。
- 参数通过"dim1,dim2,dim3;dim4,dim5,dim6;dim7,dim8,dim9"的形式设置，所有档位必须放在双引号中，每档中间使用英文分号分隔，每档中的dim值与INPUT\_SHAPE参数中的-1标识的参数依次对应，INPUT\_SHAPE参数中有几个-1，则每档必须设置几个维度。例如：

    ```c++
    void PrepareOptions(std::map<std::string, std::string>& options) {
        options.insert({
            {ge::ir_option::INPUT_FORMAT, "ND"},
            {ge::ir_option::INPUT_SHAPE, "data:1,1,40,-1;label:1,-1;mask:-1,-1"},
            {ge::ir_option::DYNAMIC_DIMS, "20,20,1,1;40,40,2,2;80,60,4,4"}
        });
    }
    ```

    则模型编译时，支持的输入shape为：

    第0档：data\(1,1,40,20\)+label\(1,20\)+mask\(1,1\)

    第1档：data\(1,1,40,40\)+label\(1,40\)+mask\(2,2\)

    第2档：data\(1,1,40,80\)+label\(1,60\)+mask\(4,4\)

- 如果模型编译时通过该参数设置了动态维度，则使用应用工程进行模型推理时，在**模型执行**接口之前：

  - 使用[aclmdlSetInputDynamicDims](../../../api/graph_engine_api/c/acl/aclmdlSetInputDynamicDims.md)接口，用于设置真实的维度。
  - 不使用[aclmdlSetInputDynamicDims](../../../api/graph_engine_api/c/acl/aclmdlSetInputDynamicDims.md)接口，则模型执行时，默认按照动态维度设置范围的最大值进行赋值。

### 编译并运行Graph场景

当前系统支持在用户脚本中配置动态维度信息，从而支持动态输入的场景，本节给出具体说明。

#### 功能介绍

<!-- npu="310b" id3 -->
Atlas 200I/500 A2 推理产品不支持该特性。
<!-- end id3 -->

编译并运行Graph场景的动态维度当前仅支持在整图设置：使用Session参数设置维度信息，输入可以为dataset方式、placeholder方式，或者两种混合方式。对于混合输入，当前仅支持其中一种为动态变化的场景。

#### 使用方法

1. 在Data算子定义时，将数据shape的动态维度设置为-1：

    ```c++
    auto shape_data = vector<int64_t>({ 1,-1,-1 });
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_ND, DT_FLOAT);
    auto data = op::Data("data");
    data.update_input_desc_data(desc_data);
    data.update_output_desc_out(desc_data);
    ```

2. 编译并运行Graph时，在Session接口和AddGraph接口的options中设置ge.inputShape/ge.dynamicNodeType信息，同时通过ge.dynamicDims指定维度信息。

    > [!NOTE]说明
    >
    >- ge.inputShape参数必须设置。
    >- 整图设置动态维度时，用户在脚本中设置的ge.inputShape的输入顺序要和实际data节点的name字母序保持一致，比如有三个输入：label、data、mask，则ge.inputShape输入顺序应该为data、label、mask：
    >
    >    ```c++
    >    "data:1,1,40,-1;label:1,-1;mask:-1,-1"
    >    ```
    >
    >- 编译并运行Graph动态维度场景，如果模型有多个输入，非分档输入支持指定内部Format格式，分档输入不支持指定内部Format格式，详细使用方法请参见[指定Graph输入输出的内部格式](specify_graph_io_format.md)。

    ```c++
    std::map<std::string, std::string> options = {{"ge.inputShape", "data:1,1,40,-1;label:1,-1;mask:-1,-1"},
                                                 {"ge.dynamicDims", "20,20,1,1;40,40,2,2;80,60,4,4"},
                                                 {"ge.dynamicNodeType", "1"}};
    // 动态维度参数添加到Session
    ge::GeSession *session = new ge::GeSession(options);
    // 动态维度参数添加到graph
    session->AddGraph(graph_id, graph, options);
    ```

- ge.inputShape表示网络的输入shape信息，以上配置表示网络中有三个输入，输入的name分别为data，label，mask，各输入的shape分别为（1,1,40,-1）、（1,-1）、（-1,-1），其中-1表示该维度为动态，需要通过ge.dynamicDims设置动态维度参数。
- ge.dynamicDims用来设置动态维度的档位，档位中间使用英文分号分隔，每档中的dim值与ge.inputShape参数中的-1标识的参数依次对应，ge.inputShape参数中有几个-1，则每档必须设置几个维度。结合ge.inputShape信息，ge.dynamicDims配置为"20,20,1,1;40,40,2,2;80,60,4,4"，则表示输入shape支持三个档位，模型编译时，支持的输入组合档位数分别为：
  - 第0档：data\(1,1,40,20\)，label\(1,20\)，mask\(1,1\)
  - 第1档：data\(1,1,40,40\)，label\(1,40\)，mask\(2,2\)
  - 第2档：data\(1,1,40,80\)，label\(1,60\)，mask\(4,4\)

- ge.dynamicNodeType用于指定动态输入的节点类型。0：dataset输入为动态输入；1：placeholder输入为动态输入。当前不支持dataset和placeholder输入同时为动态输入。

## 动态输入shape范围

本节介绍如何在模型构建时设置动态输入的shape范围。

### 功能介绍

<!-- npu="310b" id4 -->
Atlas 200I/500 A2 推理产品不支持该特性。
<!-- end id4 -->

用户在模型编译时可以指定模型输入数据的shape范围，从而编译出支持动态输入的模型。

### 使用方法

1. 在Data算子定义时，将数据shape的指定维度设置为-1：

    ```c++
    auto shape_data = vector<int64_t>({ -1,3,5,-1 });
    TensorDesc desc_data(ge::Shape(shape_data), FORMAT_NCHW, DT_FLOAT);
    auto data = op::Data("data");
    data.update_input_desc_x(desc_data);
    data.update_output_desc_y(desc_data);
    ```

2. 模型编译时，在aclgrphBuildModel接口options中通过INPUT\_SHAPE指定模型输入数据的shape范围。

    ```c++
    void PrepareOptions(std::map<AscendString, AscendString>& options) {
        options.insert({
            {ge::ir_option::INPUT_FORMAT, "NCHW"},
            {ge::ir_option::INPUT_SHAPE, "8~20,3,5,-1"}
        });
    }
    ```

    INPUT\_SHAPE参数设置shape范围时的格式要求：

    - 支持按照name设置："input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2"，例如："input\_name1:8\~20,3,5,-1;input\_name2:5,3\~9,10,-1"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称。如果用户知道data节点的name，推荐按照name设置。
    - 支持按照index设置："n1,c1,h1,w1;n2,c2,h2,w2"，例如："8\~20,3,5,-1;5,3\~9,10,-1"。可以不指定节点名，节点按照索引顺序排列，节点中间使用英文分号分隔。按照index设置shape范围时，data节点需要设置属性index，说明是第几个输入，index从0开始。
    - 动态维度有shape范围的用波浪号“\~”表示，固定维度用固定数字表示，无限定范围的用-1表示。

### 使用注意事项

如果模型编译时通过该参数设置了动态输入的shape范围，使用应用工程进行模型推理时，需在模型执行aclmdlExecute接口之前，调用aclmdlSetDatasetTensorDesc接口，用于设置真实的输入Tensor描述信息（输入shape范围）；模型执行之后，调用aclmdlGetDatasetTensorDesc接口获取模型动态输出的Tensor描述信息；再进一步调用aclTensorDesc下的操作接口获取输出Tensor数据占用的内存大小、Tensor的Format信息、Tensor的维度信息等。
