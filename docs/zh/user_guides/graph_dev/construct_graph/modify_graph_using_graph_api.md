# 使用改图接口修改Graph

如果用户想要直接优化图的结构，比如将某些特定子图替换成一个融合算子，以减少计算步骤、外存访问、调度时间等，或者在某些算子之间添加一个算子，此时可以通过本节内容将图直接修改成期望的结构。

## 功能介绍

本节以在算子A和算子B之间添加算子C为例，说明如何修改Graph，涉及的主要接口如下：

![功能介绍](../figures/feature_intro_1.png)

本手册除了通过改图接口修改Graph，还提供了将改图函数封装为自定义Pass来修改Graph的方式，详细介绍请参见[基于改图接口实现Pass](../custom_pass_development/implement_pass_using_graph_api.md)。

## 开发示例

1. 包含的头文件。

    ```c++
    #include "graph.h"
    #include "ascend_string.h"
    #include "ge_ir_build.h"
    #include "gnode.h"
    ```

2. （可选步骤）修改图之前，可以先调用[aclgrphDumpGraph](../../../api/graph_engine_api/cpp/ge/aclgrphDumpGraph.md)把Graph dump到本地，查看Graph信息。

    需要注意的是，aclgrphDumpGraph接口必须在SetInputs接口和SetOutputs接口之后调用，例如：

    ```c++
        string op_name = "tc_ge_openpass_0001";
        // 创建Graph对象
        Graph graph(op_name);
        // 创建Data算子实例
        auto data = op::Data("data").set_attr_index(0);
        // 定义数据张量的描述信息
        TensorDesc data_desc2(ge::Shape({3, 3, 3, 3}), FORMAT_NHWC, DT_FLOAT);
        data.update_input_desc_x(data_desc2);
        data.update_output_desc_y(data_desc2);
        // 创建MatrixInverse算子实例，并设置其输入为Data的输出
        auto matrixinverse = op::MatrixInverse("MatrixInverse").set_input_x(data);
        // 创建Square算子实例，并设置其输入为MatrixInverse的输出
        auto square1 = op::Square("square1").set_input_x(matrixinverse);
        std::vector<Operator> inputs{data};
        std::vector<Operator> outputs{data,square1};
        // 调用接口，设置Graph的输入输出算子
        graph.SetInputs(inputs).SetOutputs(outputs);
        std::map<std::string, std::string> init_options = {
             {ge::ir_option::SOC_VERSION,"xxx"}
        };
        // 模型初始化，申请资源
        auto ret = aclgrphBuildInitialize(init_options);
        EXPECT_EQ(ret, GRAPH_SUCCESS);
        std::cout << "BuildInitialize before infershape Success." << std::endl;
        size_t filesize =24;
        const char* file = "tc_ge_openpass_0001_dump";
        // 将输入的Graph导出到文件中
        ret = ge::aclgrphDumpGraph(graph,file,filesize);
        if(ret != GRAPH_SUCCESS) {
            std::cout<<"dump graph failed."<<std::endl;
        }
        // 编译生成离线模型并保存到内存缓冲区
        ret = aclgrphBuildModel(graph,op_name);
        if(ret != GRAPH_SUCCESS) {
            std::cout<<"aclgrphBuildModel failed."<<std::endl;
        }
    ```

3. 在算子A和算子B之间增加算子C，比如在Const和Add算子之间插入Abs。

    ```c++
    const std::string CONST = "Const";
    const std::string ADD = "Add";
    GNode src_node;
    GNode dst_node;
    std::vector<GNode> nodes = graph.GetAllNodes();
    for(auto &node : nodes) {
      ge::AscendString name;
      node.GetName(name);
      std::string node_name(name.GetString());
      if(node_name == CONST) { src_node = node;}
      else if(node_name == ADD) { dst_node = node;}
    }
    graph.RemoveEdge(src_node, 0, dst_node, 0);
    auto abs = op::Abs("input3_abs");
    GNode node_abs = graph.AddNodeByOp(abs);
    TensorDesc output_tensor_desc;
    src_node.GetOutputDesc(0, output_tensor_desc);
    abs.UpdateInputDesc(0,  output_tensor_desc);
    abs.UpdateOutputDesc(0, output_tensor_desc);
    graph.AddDataEdge(src_node, 0, node_abs, 0);
    graph.AddDataEdge(node_abs, 0, dst_node, 0);
    ```

    1. 调用[GetAllNodes](../../../api/graph_engine_api/cpp/ge/Graph/GetAllNodes.md)找到Const算子和Add算子。
    2. 调用[RemoveEdge](../../../api/graph_engine_api/cpp/ge/Graph/RemoveEdge.md)删除Const算子和Add算子的连边（数据边或控制边）。
    3. 参考[使用算子原型衍生接口定义算子](construct_graph_using_op_proto.md#使用算子原型衍生接口定义算子)，创建Operator类算子Abs（也可以调用OperatorFactory::CreateOperator创建算子）。
    4. 调用[AddNodeByOp](../../../api/graph_engine_api/cpp/ge/Graph/AddNodeByOp.md)创建GNode类算子Abs。

        创建完算子后，可以根据需要更新该算子的input和output TensorDesc，一般根据源节点的Output TensorDesc更新算子abs的Input TensorDesc和Output TensorDesc。如果不更新，系统会设置默认值，在模型编译时对Tensor Shape，type进行推导。

    5. 调用[AddDataEdge](../../../api/graph_engine_api/cpp/ge/Graph/AddDataEdge.md)添加Const算子和Abs算子，Abs算子和Add算子之间的连边。如果有控制边，再调用[AddControlEdge](../../../api/graph_engine_api/cpp/ge/Graph/AddControlEdge.md)添加控制边。

    如果在A与B插入多个算子，比如，A-\>C-\>D-\>B，参考以上步骤，分别执行操作A-\>C, C-\>D, D-\>B。

4. 删除算子A和算子B之间的C算子，比如删除算子Const和Add之间的Abs。

    ```c++
    graph.RemoveNode(node_abs);
    graph.AddDataEdge(src_node, 0, dst_node, 0);
    ```

    1. 调用[RemoveNode](../../../api/graph_engine_api/cpp/ge/Graph/RemoveNode.md)删除Abs算子。
    2. 调用[AddDataEdge](../../../api/graph_engine_api/cpp/ge/Graph/AddDataEdge.md)添加Const和Add算子之间的连边。如果有控制边，再调用[AddControlEdge](../../../api/graph_engine_api/cpp/ge/Graph/AddControlEdge.md)添加控制边。

5. 此外，如果需要查询GNode的信息，可以参考[GNode](../../../api/graph_engine_api/cpp/ge/GNode/GNode.md)提供的方法。
