# 通过算子原型构建Graph

本节介绍如何通过算子原型和图引擎接口，一步步地构建完整Graph。

## 功能介绍

使用REG\_OP宏将算子原型注册成功后，会自动生成对应的衍生接口（参见[原型定义衍生接口](../../../api/graph_engine_api/cpp/ge/prototype_definition_derived_interface.md)），用户可以通过这些接口在Graph中定义算子，随后创建一个Graph实例，并在Graph中设置输入算子、输出算子，从而完成Graph构建。

![图示](../figures/feature_intro.png)

## 使用算子原型衍生接口定义算子

下面仅介绍使用算子原型衍生接口定义算子的通用过程，具体各类算子（例如数据节点、计算节点）的定义示例，请参考[Graph中各类算子表达样例](op_expr_samples_in_graph.md)。

1. 包含的头文件。

    ```c++
    # 必选头文件
    #include "ops_proto_legacy.h"
    # 可选头文件
    #include "ops_proto_math.h"
    #include "ops_proto_cv.h"
    #include "ops_proto_nn.h"
    #include "ops_proto_transformer.h"
    ```

    - 对于内置算子，定义内置算子类型，可使用内置算子类型相关的接口，头文件所在路径为“${INSTALL_DIR}/opp/built-in/op_graph/inc/”。

        开发时必须包含ops_proto_legacy.h头文件。此外，请根据具体算子按需包含对应的可选头文件。例如，若使用SoftmaxV2算子，则需额外包含 ops_proto_nn.h。

        ```c++
        #include "ops_proto_legacy.h"
        #include "ops_proto_nn.h"
        ```

    - 对于自定义算子，需要包括自定义算子的原型定义头文件，头文件所在路径为“$\{INSTALL\_DIR\}/opp/vendors/<vendor\_name\>/op\_proto/inc”。

    其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

2. 创建算子实例。

    使用REG\_OP宏注册算子类型后，会自动生成算子类型构造函数explicit OpType\(const char\* name\)，相当于定义了一个op::xxx的Class，开发者include该原型头文件，实例化该Class构建Graph。在构建Graph时可直接传算子名称作为入参，例如：

    ```c++
    auto softmax = op::SoftmaxV2("softmax")
    ```

    注意：图中的算子名称**必须唯一**。

3. 设置算子输入。

    算子原型中定义了算子的输入名称、输入类型，以及算子支持的数据类型。根据输入类型，可将算子输入分为可选输入、必选输入和动态输入。

    对于可选输入、必选输入和动态输入，需要通过不同的接口设置。

    - 设置算子**必选输入和可选输入**：通过“set\_input\__输入名称_”设置，例如：

        ```c++
        auto softmax = op::SoftmaxV2("softmax")         // 创建SoftmaxV2算子实例
          .set_input_x(bias_add_3);                     // 设置SoftmaxV2算子输入为bias_add_3
        ```

    - 设置算子**动态输入**：通过“create\_dynamic\_input\__输入名称_”创建动态输入、“set\_dynamic\_input\__输入名称_”设置动态输入，示例请参考[定义动态多输入算子（AddN）](./op_expr_samples_in_graph.md#定义动态多输入算子addn)。

4. 设置算子属性。

    算子原型中定义了算子的属性名称、属性类型、属性支持的数据类型、属性的默认值及取值范围。根据属性类型，可将算子属性分为：必选属性（REQUIRED\_ATTR，一定要在算子定义时设置属性值）和可选属性（ATTR，不设置算子对象的属性值时使用默认值）。

    对于**必选属性和可选属性**，都可以通过“set\_attr\__属性名称_”接口设置，例如：

    ```c++
    auto maxpool1 = op::MaxPool("MaxPool1")
    .set_input_x(tanh1)
    .set_attr_ksize({1, 1, 2, 1})                // 设置ksize属性值
    .set_attr_strides({1, 1, 2, 1})              // 设置strides属性值
    .set_attr_padding("SAME");                   // 设置padding属性值
    ```

## 算子连接边表达

算子之间的连边分为数据边和控制边。数据边用于指定算子的输入，控制边用于控制算子的执行顺序。

1. 数据边表达。

    对于前一个算子只有一个输出，可以通过“set\_input\_输入名称”接口传入前一个算子的名称。

    ![数据边表达1](../figures/data_edge_expression_1.png)

    ```c++
    auto bias_add_3 = op::BiasAdd("bias_add_3")
      .set_input_x(matmul_2)
      .set_input_bias(bias_add_const_3)
      .set_attr_data_format("NCHW");
    auto softmax = op::SoftmaxV2("Softmax")
      .set_input_x(bias_add_3);
    ```

    如果前一个算子有多个输出，则需要传入上一个算子的名称和输出名称，或者传入上一个算子的名称和输出索引。

    ![数据边表达2](../figures/data_edge_expression_2.png)

    传入上一个算子的名称和输出名称：

    ```c++
    auto data = op::Data("data");
    auto unique= op::Unique("unique").set_input_x(data);
    auto softplus = op::Softplus("softplus").set_input_x(unique, "y");  // 创建softplus算子，设置输入为unique算子的y输出
    auto sqrt = op::Sqrt("sqrt").set_input_x(unique, "idx");            // 创建sqrt算子，设置输入为unique算子的idx输出
    ```

    传入上一个算子的名称和输出索引：

    ```c++
    auto data = op::Data("data");
    auto unique= op::Unique("unique").set_input_x(data);
    auto softplus = op::Softplus("softplus").set_input_x(unique, 0);  // 创建softplus算子，设置输入为unique算子的第一个输出
    auto sqrt = op::Sqrt("sqrt").set_input_x(unique, 1);              // 创建sqrt算子，设置输入为unique算子的第二个输出
    ```

2. 控制边表达。

    如果图中某个算子的执行依赖于图中其他算子执行完，如下图所示，如果需要控制先执行Sqrt再执行Softplus，则需要调用[AddControlInput](../../../api/graph_engine_api/cpp/ge/Operator/AddControlInput.md)接口，对Softplus算子增加控制边。

    ![控制边表达](../figures/control_edge_expression.png)

    代码示例：

    ```c++
    auto data = op::Data("data");
    auto unique= op::Unique("unique").set_input_x(data);
    auto sqrt = op::Sqrt("sqrt").set_input_x(unique, "idx");
    auto softplus = op::Softplus("softplus").set_input_x(unique, "y").AddControlInput(sqrt);
    ```

## 创建Graph实例

完成算子定义后，需要创建Graph实例，并在Graph中设置输入算子、输出算子，主要过程为：

1. 包含所需的头文件。

    ```c++
    #include "graph.h"
    ```

2. 创建Graph对象。

    ```c++
    Graph graph("IrGraph");
    ```

    相关接口请参考[Graph](../../../api/graph_engine_api/cpp/ge/Graph/overview.md)。

3. 设置Graph输入和输出算子，使用到的主要接口为：

    - 设置Graph内的输入算子：[SetInputs](../../../api/graph_engine_api/cpp/ge/Graph/SetInputs.md)
    - 设置Graph内的输出算子：[SetOutputs](../../../api/graph_engine_api/cpp/ge/Graph/SetOutputs.md)

    例如设置Graph的输入为Data算子，输出为Softmax算子：

    ```c++
    std::vector<Operator> inputs{data};      // data为Data类型的算子对象
    std::vector<Operator> outputs{softmax};  // softmax为Softmax类型的算子对象
    graph.SetInputs(inputs).SetOutputs(outputs);
    ```

    如果输入为多个Data算子，需要保证inputs入参顺序和Data算子index属性指定的顺序保持一致，否则后面生成模型时会报错。例如：

    ```c++
    // 准备第一个输入数据
    auto shape_data0 = vector<int64_t>({1,17,2,2});
    TensorDesc desc_data0(ge::Shape(shape_data0), FORMAT_ND, DT_FLOAT);
    auto data0 = op::Data("data0").set_attr_index(0) ;   // 创建data0算子，index属性为0
    data0.update_input_desc_x(desc_data0);               // 设置算子输入描述
    data0.update_output_desc_y(desc_data0);              // 设置算子输出描述
    // 准备第二个输入数据
    auto shape_data1 = vector<int64_t>({1,5,2,2});
    TensorDesc desc_data1(ge::Shape(shape_data1), FORMAT_ND, DT_FLOAT);
    auto data1 = op::Data("data1").set_attr_index(1) ;   // 创建data1算子，index属性为1
    data1.update_input_desc_x(desc_data1);               // 设置算子输入描述
    data1.update_output_desc_y(desc_data1);              // 设置算子输出描述

    // 设置Graph输入算子
    std::vector<Operator> inputs{data0, data1};
    ```

    >[!NOTE]说明
    >
    >在构图过程中，如果Tensor的shape维度和format维度数量不一致，按照如下表格中的规则理解当前维度：
    >例如，shape只有1维为\[16\]，format为4维，比如NHWC，该场景下可以理解为shape的1维为C轴，其他轴需要补维，补维后格式为\[1,1,1,16\]；
    >shape为2维\[16,16\]，format为4维，比如NHWC，该场景下可以理解为shape的2维为HW轴，其他轴需要补维，补维后格式为\[1,16,16,1\]。
    > <!-- npu="950" id1 -->
    >该说明**不适用于**Ascend 950PR/Ascend 950DT。
    > <!-- end id1 -->

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
