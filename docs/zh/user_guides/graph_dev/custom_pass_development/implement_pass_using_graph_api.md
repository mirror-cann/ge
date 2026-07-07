# 基于改图接口实现Pass

本节介绍如何通过改图接口封装的自定义Pass，实现对Graph的修改。

Graph的修改功能主要支持两种调用方式：

- 直接调用接口：用户在构建完Graph后，可直接调用改图接口（详见[使用改图接口修改Graph](../construct_graph/modify_graph_using_graph_api.md)），实时对图结构进行调整。该种方式适用于简单、一次性修改场景。
- 自定义Pass方式：更推荐的方式是将改图函数封装为自定义Pass，通过[REGISTER\_CUSTOM\_PASS](../../../api/graph_engine_api/cpp/ge/REGISTER_CUSTOM_PASS.md)宏完成注册。注册后的Pass可编译为动态库插件，在图优化流程的指定阶段被调用，实现模块化、可复用、可配置的图变换能力。

本章将详细介绍如何开发、注册并集成自定义Pass，助力用户实现高效、灵活的Graph优化策略。

## 功能介绍

一个改图函数可看作是一个自定义Pass，用户可调用REGISTER\_CUSTOM\_PASS注册宏，按照指定的Pass名称完成Pass的注册，通过把改图函数编译成动态库插件的方式，注册的Pass在指定阶段被调用，示例代码如下：

```c++
#include "register_custom_pass.h"
// 用户自定义改图函数
graphStatus CustomPassFunc(GraphPtr &graph, CustomPassContext &custom_context) {
    // 此处定义图修改具体行为
    return GRAPH_SUCCESS;
}
// 改图Pass注册，注册后的Pass在指定阶段被调用
REGISTER_CUSTOM_PASS("pass_name").CustomPassFn(CustomPassFunc).Stage(CustomPassStage::kBeforeInferShape);
```

- register\_custom\_pass.h：存储在“CANN软件安装目录/cann/include/register/”目录下，包含该头文件，可使用Pass注册相关类，使用Pass注册相关接口。
- graphStatus：成功返回ge::GRAPH\_SUCCESS，其他返回值均为失败。建议使用小于0的值作为返回的错误码，大于0的值可能会和框架已使用的错误码产生冲突。
- CustomPassFunc：自定义Pass的入口函数，详情请参见[回调函数CustomPassFunc](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/CustomPassFn.md)。
- graph：自定义Pass要修改的图，类型为GraphPtr。
- custom\_context：CustomPassContext类对象，可参考[CustomPassContext](../../../api/graph_engine_api/cpp/ge/CustomPassContext/CustomPassContext.md)提供的方法。
- REGISTER\_CUSTOM\_PASS：注册自定义Pass，"pass\_name"可任意命名，详情请参见[REGISTER\_CUSTOM\_PASS](../../../api/graph_engine_api/cpp/ge/REGISTER_CUSTOM_PASS.md)。
- CustomPassFn：自定义Pass的执行函数，详情请参见[CustomPassFn](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/CustomPassFn.md)。
- Stage：指定Pass执行阶段，详情请参见[Stage](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/Stage.md)。

> [!NOTE]说明
>
>如果用户在改图过程中，需要替换成其他功能的算子，但是该算子CANN不支持，可以通过如下方式自定义该算子：
>通过Ascend C自定义该算子，详情请参见《Ascend C算子开发指南》。
>通过TBE自定义该算子，详情请参见《TBE&AI CPU算子开发》。
>将该算子开发完成后，才能正常使用自定义Pass修改Graph的功能。

## 开发示例

此处以MatMul+Add结构融合为GEMM自定义Pass为例，详细介绍如何通过自定义Pass修改Graph，详细可以参见[样例源码](https://gitee.com/ascend/samples/blob/master/cplusplus/level1_single_api/3_ir/2_fuse_matmul_add_pass/src/fuse_matmul_add_pass.cpp)。

样例仓还提供了如下两个使用自定义Pass修改Graph的样例，用户可以单击对应链接进行查看：

- 如何通过自定义Pass把Tile+Concat图结构修改为Concat+Tile+Concat图结构，单击[fuse\_tile\_concat\_pass](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/1_fuse_tile_concat_pass)获取链接。
- 如何使用自定义Pass修改子图，在子图的Data+FrameworkOp结构中间插入Abs节点，单击[modify\_subgraph\_pass](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/3_modify_subgraph_pass)获取样例。

1. 包含的头文件。

    ```c++
    #include <iostream>
    // 自定义Pass接口头文件
    #include "register_custom_pass.h"
    // 新增算子头文件
    #include "all_ops.h"
    // 如果使用Ascend C自定义了算子，需要包含如下头文件(如果不支持Ascend C自定义算子，则请忽略如下头文件)：
    #include "<CANN软件安装目录>/cann/opp/vendors/<customize>/op_proto/inc/op_proto.h"
    ```

    其中：<CANN软件安装目录\>：请修改为CANN软件包的实际安装路径，<customize\>：请修改为Ascend C自定义算子实际工程名。

2. 使用自定义Pass修改Graph。（如下代码仅为示例，不可执行；修改Graph时，只能使用[CustomPassContext](../../../api/graph_engine_api/cpp/ge/CustomPassContext/CustomPassContext.md)、[Graph](../../../api/graph_engine_api/cpp/ge/Graph/Graph.md)、[GNode](../../../api/graph_engine_api/cpp/ge/GNode/GNode.md)、[PassReceiver](../../../api/graph_engine_api/cpp/ge/PassReceiver/PassReceiver.md)、[PassRegistrationData](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/PassRegistrationData.md)、[REGISTER\_CUSTOM\_PASS](../../../api/graph_engine_api/cpp/ge/REGISTER_CUSTOM_PASS.md)、[StreamPassContext](../../../api/graph_engine_api/cpp/ge/StreamPassContext/overview.md)中的接口）

    ```c++
    namespace {
    constexpr const char *kOpNameAdd = "add";
    constexpr const char *kOpNameMatMul = "matmul";
    constexpr const char *kOpNameGEMM = "gemm";
    constexpr const char *kOpNameAlpha = "alpha";
    constexpr const char *kOpNameBeta = "beta";
    constexpr const char *kAttrNameTransposeA = "transpose_a";
    constexpr const char *kAttrNameTransposeB = "transpose_b";
    constexpr int32_t kIndex0 = 0;
    constexpr int32_t kIndex1 = 1;
    constexpr int32_t kIndex2 = 2;
    constexpr int32_t kIndex3 = 3;
    constexpr int32_t kIndex4 = 4;

    // 1.遍历所有节点，寻找MatMul和Add节点
    bool FindNodes(GraphPtr &graph, GNode &src_node, GNode &dst_node) {
        auto all_nodes = graph->GetAllNodes();
        bool find_src_node = false;
        bool find_dst_node = false;
        for (auto &node: all_nodes) {
            AscendString node_name;
            auto ret = node.GetName(node_name);
            // 找到MatMul
            if (node_name == kOpNameMatMul) {
                src_node = node;
                find_src_node = true;
                cout << "Find src node: MatMul." << endl;
            // 找到Add
            } else if (node_name == kOpNameAdd) {
                dst_node = node;
                find_dst_node = true;
                cout << "Find dst node: Add." << endl;
            }
        }
        return (find_src_node && find_dst_node);
    }
    // 2.判断MatMul和Add节点是否有连边关系
    bool CheckNodesHaveEdge(GraphPtr &graph, const GNode &src_node, const GNode &dst_node) {
        for (auto &[out_node, _]: src_node.GetOutDataNodesAndPortIndexs(kIndex0)) {
            AscendString node_name;
            auto ret = out_node->GetName(node_name);
            if (node_name == kOpNameAdd) {
                return true; // 直接相连
            }
        }
        return false;
    }
    // 3.创建和添加GEMM节点
    void CreateGEMMNode(GraphPtr &graph, const GNode &src_node, GNode &node_gemm) {
        bool transpose_a = false;
        bool transpose_b = false;
        src_node.GetAttr(kAttrNameTransposeA, transpose_a);
        src_node.GetAttr(kAttrNameTransposeB, transpose_b);
        constexpr float kValue1 = 1;
        TensorDesc alpha_desc(ge::Shape({1}), FORMAT_ND, DT_FLOAT);
        Tensor alpha_tensor(alpha_desc, reinterpret_cast<const uint8_t *>(&kValue1), sizeof(float));
        auto alpha = op::Const(kOpNameAlpha).set_attr_value(alpha_tensor);
        TensorDesc beta_desc(ge::Shape({1}), FORMAT_ND, DT_FLOAT);
        Tensor beta_tensor(beta_desc, reinterpret_cast<const uint8_t *>(&kValue1), sizeof(float));
        auto beta = op::Const(kOpNameBeta).set_attr_value(beta_tensor);

        auto gemm = op::GEMM(kOpNameGEMM);
        gemm.set_attr_transpose_a(transpose_a)
            .set_attr_transpose_b(transpose_b);
        gemm.update_input_desc_alpha(alpha_desc);
        gemm.update_input_desc_beta(beta_desc);

        // 将alpha、beta、gemm节点添加到图中
        auto node_alpha = graph->AddNodeByOp(alpha);
        auto node_beta = graph->AddNodeByOp(beta);
        node_gemm = graph->AddNodeByOp(gemm);

        // 建立alpha、beta与GEMM的数据边
        auto ret = graph->AddDataEdge(node_alpha, kIndex0, node_gemm, kIndex3);
        ret = graph->AddDataEdge(node_beta, kIndex0, node_gemm, kIndex4);
    }
    // 4.添加新节点的输入输出
    bool AddInputsAndOutputs(GraphPtr &graph, const GNode &src_node, const GNode &dst_node, GNode &node_gemm) {
        auto [a, a_output_index] = src_node.GetInDataNodesAndPortIndexs(kIndex0);
        auto [b, b_output_index] = src_node.GetInDataNodesAndPortIndexs(kIndex1);
        int32_t add_node_c_input_index = -1;
        for (size_t i = 0; i < dst_node.GetInputsSize(); ++i) {
            auto [in_node, _] = dst_node.GetInDataNodesAndPortIndexs(i);
            AscendString node_name;
            auto ret = in_node->GetName(node_name);
            if (node_name != kOpNameMatMul) {
                add_node_c_input_index = i;
                break;
            }
        }
        if (add_node_c_input_index == -1) {
            return false;
        }
        auto [c, c_output_index] = dst_node.GetInDataNodesAndPortIndexs(add_node_c_input_index);
        // 建立a、b、c与GEMM的数据边
        auto ret = graph->AddDataEdge(*a, a_output_index, node_gemm, kIndex0);
        if (ret != GRAPH_SUCCESS) {
            return false;
        }
        ret = graph->AddDataEdge(*b, b_output_index, node_gemm, kIndex1);
        ret = graph->AddDataEdge(*c, c_output_index, node_gemm, kIndex2);

        // 更新GEMM的输入/输出描述信息
        TensorDesc input_desc_a;
        ret = src_node.GetInputDesc(kIndex0, input_desc_a);
        ret = node_gemm.UpdateInputDesc(kIndex0, input_desc_a);

        TensorDesc input_desc_b;
        ret = src_node.GetInputDesc(kIndex1, input_desc_b);
        ret = node_gemm.UpdateInputDesc(kIndex1, input_desc_b);

        TensorDesc input_desc_c;
        ret = dst_node.GetInputDesc(add_node_c_input_index, input_desc_c);
        ret = node_gemm.UpdateInputDesc(kIndex2, input_desc_c);

        TensorDesc output_desc_y;
        ret = dst_node.GetOutputDesc(kIndex0, output_desc_y);
        ret = node_gemm.UpdateOutputDesc(kIndex0, output_desc_y);
        return true;
    }
    // 5.删除旧节点和其连边关系，连接新GEMM节点和输出节点
    void RemoveOldNodesEdgesAndAddGemmOutput(GraphPtr &graph, GNode &src_node, GNode &dst_node, GNode &node_gemm) {
        vector<GNode> node_vec{src_node, dst_node};
        // 删除旧节点的所有输入边
        for (auto &node: node_vec) {
            for (size_t i = 0; i < node.GetInputsSize(); ++i) {
                auto [in_node, in_id] = node.GetInDataNodesAndPortIndexs(i);
                if (in_node != nullptr) {
                    auto ret = graph->RemoveEdge(*in_node, in_id, node, i);
                }
            }
        }

        // 把旧节点的输出边改为连接到新GEMM
        for (auto &[out_node, out_id]: dst_node.GetOutDataNodesAndPortIndexs(kIndex0)) {
            if (out_node != nullptr) {
                auto ret = graph->RemoveEdge(dst_node, kIndex0, *out_node, out_id);
                ret = graph->AddDataEdge(node_gemm, kIndex0, *out_node, out_id);
            }
        }
        // 最后删除旧节点
        for (auto &node: node_vec) {
            auto ret = graph->RemoveNode(node);
        }
    }
    } // namespace

    // |o>-----------------------------------
    // |o>    a  b
    // |o>    \ /              a   b    c
    // |o>   MatMul  c   ==>   \   |   /
    // |o>     \    /            GEMM
    // |o>      Add
    // |o>-----------------------------------
    // 融合说明：本例识别上图中左边的MatMul+Add结构并通过图修改接口替换为右边的单个GEMM节点
    // 改图接口返回值说明：本文件中的改图接口需要判断返回值, 基于可读性考虑除了Pass入口函数外，其他函数中的改图接口只接收返回值，但不增加返回值处理代码；如需判断返回值，可配合使用custom_context.SetErrorMessage("xxx")方法
    ```

3. 自定义改图Pass注册。

    ```c++
    graphStatus FuseMatMulAndAddPass(GraphPtr &graph, CustomPassContext &custom_context) {
        cout << "FuseMatMulAndAddPass begin." << endl;
        GNode src_node;
        GNode dst_node;
        // 1.遍历所有节点，查找MatMul和Add节点
        if (!FindNodes(graph, src_node, dst_node)) {
            cout << "Do not find MatMul or Add node." << endl;
            return GRAPH_SUCCESS;
        }

        // 2.判断MatMul和Add节点是否有连边关系
        if (!CheckNodesHaveEdge(graph, src_node, dst_node)) {
            cout << "There is no edge between src and dst node." << endl;
            return GRAPH_SUCCESS;
        }

        // 3.创建和添加GEMM节点
        GNode node_gemm;
        CreateGEMMNode(graph, src_node, node_gemm);

        // 4.添加新节点的输入输出
        if (!AddInputsAndOutputs(graph, src_node, dst_node, node_gemm)) {
            custom_context.SetErrorMessage("Add inputs and outputs failed.");
            return -1;
        }

        // 5.删除旧节点和其连接边关系，连接新GEMM节点和输出节点
        RemoveOldNodesEdgesAndAddGemmOutput(graph, src_node, dst_node, node_gemm);

        cout << "FuseMatMulAndAddPass end." << endl;
        return GRAPH_SUCCESS;
    }
    // 使用REGISTER_CUSTOM_PASS注册宏进行改图Pass注册，并指定被调用的阶段
    REGISTER_CUSTOM_PASS("FuseMatMulAndAddPass").CustomPassFn(FuseMatMulAndAddPass).Stage(CustomPassStage::kBeforeInferShape);
    ```

## 如何使用自定义Pass

完成上述自定义Pass后，本节简单介绍如何把改图函数编译成动态库插件方式，以便注册的Pass在图编译的最开始被框架调用。详细使用说明请参见[样例使用指导](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/2_fuse_matmul_add_pass)。

1. 把[开发示例](#开发示例)中的改图函数编译成仅以".so"结尾的动态库文件。
2. 把上述".so"动态库文件复制到$\{INSTALL\_DIR\}/opp/vendors/_xxx_/custom\_fusion\_passes/目录下。（支持设置软链接的方式；".so"文件对执行用户，需要有可读权限）

    多个"$\{INSTALL\_DIR\}/opp/vendors/_xxx_"目录按照文本序排序后遍历寻找"custom\_fusion\_passes/"子目录，单个子目录内的".so"按照文本序加载，非".so"结尾的文件在加载时跳过。

    - 其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。
    - _xxx_：有且仅有一层自定义目录。
    - custom\_fusion\_passes：该目录下不能有子目录。

3. 支持但不限于如下几种入口编译模型文件：

    如果要查看上述自定义Pass有没有生效，在编译模型前，需要dump图进行查看：在执行之前设置DUMP\_GE\_GRAPH（详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)）环境变量，然后使用如下入口编译模型：

    - 使用ATC工具进行模型转换。ATC工具使用方法请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。
    - [编译Graph为离线模型](../compile_and_run_graph/compile_graph_to_offline_model.md)。
    - [编译并运行Graph](../compile_and_run_graph/compile_and_run_graph.md)。

## 结果验证

请参见[样例使用指导](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/2_fuse_matmul_add_pass)\>程序运行\>检查执行结果。

设置了dump环境变量后，程序执行完毕，会在当前路径生成ge\_onnx\*.pbtxt等图文件，用户可以获取如下两张图，随后使用Netron等可视化软件查看：

- 指定Pass执行阶段在InferShape之前：
  - ge\_onnx\_xxxx\_PreRunBegin.pbtxt：融合前的图
  - ge\_onnx\_xxxx\_RunCustomPassBeforeInfershape.pbtxt：融合后的图

    以MatMul+Add融合为GEMM自定义Pass为例，查看融合前的图结构为：

    ![图示](../figures/fusion_pass_image1.png)

    通过自定义Pass修改后的图结构如下所示，可以看出MatMul+Add结构已经替换为单个GEMM节点。

    ![图示](../figures/fusion_pass_image2.png)

- 指定Pass执行阶段在InferShape之后：

  - ge\_onnx\_xxxx\_PrepareAfterInferFormatAndShape.pbtxt：融合前的图
  - ge\_onnx\_xxxx\_RunCustomPass\_AfterInferShape.pbtxt：融合后的图

  以BatchMatMulV2融合为Transpose+Mul+ReduceSum自定义Pass为例，查看融合前的图结构为：

  ![图示](../figures/fusion_pass_image3.png)

  通过自定义Pass修改后的图结构如下所示，可以看出BatchMatMulV2已经替换为Transpose+Mul+ReduceSum三个节点。

  ![图示](../figures/fusion_pass_image4.png)

- 指定Pass执行阶段在内置的原图融合Pass之后执行：

  - ge\_onnx\_xxxx\_OptimizeOriginalGraph\_FeGraphFusionAfter.pbtxt：融合前的图
  - ge\_onnx\_xxxx\_RunCustomPassAfterBuiltinFusionPass.pbtxt：融合后的图

  以在子图的Data-Sqrt结构中间插入Abs算子为例，查看融合前的图结构为：

  ![图示](../figures/fusion_pass_image5.png)

  通过自定义Pass修改后的图结构如下所示，可以看出Abs算子已经插入Data-Sqrt结构。

  ![图示](../figures/fusion_pass_image6.png)
