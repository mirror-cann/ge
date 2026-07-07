# 使用自定义逻辑流分配Pass定制并发

## 功能介绍

由于算子执行时间、核占用、带宽等执行时的因素影响，导致一套算法在不同硬件上并发效果不同，并且在编译阶段无法准确预估；目前并不存在一个完美的算法，在任意网络、任意硬件上都能达到最佳并发。

基于上述问题，引入该章节的特性，开放自定义逻辑流分配Pass的接入，用户可以结合图的拓扑结构、根据Profiling来分析具体网络在具体硬件上的硬件利用率，从而可以灵活地调整并发效果，一网一策，一卡一策，达到最优性能。使用自定义逻辑流分配Pass定制并发，一般有两种使用场景：

- 基于内置的逻辑流分配结果进行微调。
- 基于图结构开发自研的流分配算法。

相关概念：

- 逻辑流：与物理流对应的概念，在图上按某些条件（拓扑序、引擎归属）预分配的流，这条逻辑流上的任务将按先后顺序执行；本文讲述的流均为逻辑流。
- 流分配：指定某几个任务可以并行执行，可以提升硬件利用率，降低模型执行时间。

本章节以场景一“基于内置的逻辑流分配结果进行微调”为例，详细介绍如何使用自定义逻辑流分配Pass定制并发，整体流程为：

首先，用户可以调用[REGISTER\_CUSTOM\_PASS](../../../api/graph_engine_api/cpp/ge/REGISTER_CUSTOM_PASS.md)注册宏，按照指定的Pass名称完成Pass的注册；其次，通过把函数编译成动态库插件方式，注册的Pass在逻辑流分配阶段之后被调用；最终，用户可以基于内置的流分配结果进行微调，示例代码如下：

```c++
#include "register_custom_pass.h"
// 用户自定义逻辑流分配函数
Status CustomStreamPassFunc(const ConstGraphPtr &graph, StreamPassContext &stream_context) {
    // 在此函数中定义逻辑流分配行为
    return SUCCESS;
}
// Pass注册，无需指定stage，默认在逻辑流分配阶段之后执行
REGISTER_CUSTOM_PASS("pass_name").CustomAllocateStreamPassFn(CustomStreamPassFunc);
```

- register\_custom\_pass.h：存储在“CANN软件安装目录/cann/include/register/”目录下，包含该头文件，可使用Pass注册相关类，使用Pass注册相关接口。
- Status：成功返回ge::GRAPH\_SUCCESS，返回其他全为失败。建议使用小于0的值作为返回的错误码，大于0的值可能会和框架已使用的错误码产生冲突。
- CustomStreamPassFunc：自定义Pass的执行函数，详情请参见[回调函数CustomAllocateStreamPassFunc](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/CustomAllocateStreamPassFn.md#回调函数customallocatestreampassfunc)。
- graph：待分配逻辑流的图，类型为ConstGraphPtr。
- stream\_context：StreamPassContext类对象，可参考[StreamPassContext](../../../api/graph_engine_api/cpp/ge/StreamPassContext/overview.md)提供的方法。
- REGISTER\_CUSTOM\_PASS：注册自定义Pass，"pass\_name"可任意命名，详情请参见[REGISTER\_CUSTOM\_PASS](../../../api/graph_engine_api/cpp/ge/REGISTER_CUSTOM_PASS.md)。
- **CustomAllocateStreamPassFn**：注册自定义的逻辑流分配Pass执行函数，详情请参见[CustomAllocateStreamPassFn](../../../api/graph_engine_api/cpp/ge/PassRegistrationData/CustomAllocateStreamPassFn.md)。

> [!NOTE]说明
>
>开启强制单流开关时（[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/basic_functions.md)中ge.enableSingleStream设置为true），自定义逻辑流分配Pass不会被执行。

## 开发示例

- **前提条件**

    结合下图结构和Profiling分析优化点：

    **图 1**  原图<a id="fig1"></a>
    ![图示](../figures/original_image.png "原图")

    其Profiling分析结果如下图所示：由Profiling分析结果可知**有并发条件（无数据依赖和控制依赖）**的算子1和2为串行执行（在逻辑流分配以后，算子1和2被分配了同一个Stream ID），可以基于本节提供的功能，修改为并行执行。

    ![图示](../figures/fusion_pass_image7.png)

    >[!NOTE]说明
    >
    >有并发条件的算子若使用相同的计算资源，且存在资源的抢占和等待，并不总是能够获得并发收益，需要根据Profiling详细分析，此处仅以此为例介绍如何调整。

- **开发步骤**
    1. 包含如下头文件：

        ```c++
        #include <iostream>
        // 自定义Pass接口头文件
        #include "register_custom_pass.h"
        ```

    2. 开发自定义Pass，对节点1分配新的Stream ID。（如下代码仅为示例，不可执行）

        说明：用户获取待分配逻辑流的图后，只能对当前图上的节点做改动，即只能使用[GetDirectNode](../../../api/graph_engine_api/cpp/ge/Graph/GetDirectNode.md)接口获取本图中的所有节点。

        ```c++
        #include <iostream>
        #include "register_custom_pass.h"
        // 自定义流分配Pass
        graphStatus AllocateStreamPass(const ConstGraphPtr &graph, StreamPassContext &context) {
            // 遍历图中所有直接子节点（即不递归子图）
            for (const auto &node : graph->GetDirectNode()) {
                AscendString node_name;
                node.GetName(node_name); // 获取节点名称
                // 判断节点是否为指定的Abs_1节点
                if (std::string(node_name.GetString()) == "Abs_1/unary_ops_composition_0_SquareAbs_1/unary_ops_composition_1_Abs") {
                    // 为该节点分配一个新的Stream ID
                    context.SetStreamId(node, context.AllocateNextStreamId());
                }
            }
            return GRAPH_SUCCESS;
        }
        // 注册自定义Pass
        REGISTER_CUSTOM_PASS("AllocateStreamPass").CustomAllocateStreamPassFn(AllocateStreamPass);
        ```

## 如何使用自定义Pass

完成上述自定义Pass后，本节简单介绍如何把用户自定义逻辑流分配函数编译成动态库插件方式，以便注册的Pass在逻辑流分配阶段之后被调用。

- **前提条件**

    请参见《CANN 软件安装》安装CANN软件包。

- **程序编译：**
    1. 参见[样例使用指导](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/3_ir/2_fuse_matmul_add_pass)，获取其中的CMakeLists.txt编辑脚本，并按照Sample中的目录结构，将用户自定义逻辑流分配函数**_AllocateStreamPass.cpp_**文件放在src目录下。
    2. 根据实际情况修改CMakeLists.txt文件中的如下信息：
        - ASCEND\_PATH：指定CANN软件安装后文件存储路径，以root安装用户为例，/usr/local/Ascend/cann。
        - target\_include\_directories：需要包含的头文件，对于本示例，无需修改。如果是用户自行开发的代码，当需要添加头文件时，在示例下方直接增加行即可，注意不要删除原有项目。如果网络中有自定义算子，请增加自定义算子的原型定义头文件。
        - target\_link\_libraries：需要链接的库，对于本示例，无需修改。如果是用户自行开发的代码，当需要添加链接库时，在示例下方直接增加行即可，注意不要删除原有项目。

    3. 执行如下命令进行编译：

        ```bash
        mkdir build && cd build
        cmake .. && make
        ```

        编译结束后，在build目录下生成动态库文件**lib_AllocateStreamPass_.so**。

    4. 将lib_AllocateStreamPass_.so拷贝到$\{ASCEND\_PATH\}/opp/vendors/_xxx_/custom\_fusion\_passes/目录下。其中“_xxx_”为用户自定义目录。（支持设置软链接的方式；".so"文件对可执行用户，需要有可读权限）

        多个"$\{ASCEND\_PATH\}/opp/vendors/_xxx_"目录按照文本序排序后遍历寻找"custom\_fusion\_passes/"子目录，单个子目录内的".so"按照文本序加载，非".so"结尾的文件在加载时跳过：

        - _xxx_：有且仅有一层自定义目录。
        - custom\_fusion\_passes：该目录下不能有子目录。

- **自定义Pass使用**（支持但不限于如下几种入口编译模型文件）

    如果要查看上述自定义Pass有没有生效，在编译模型前，需要dump图进行查看：在执行之前设置DUMP\_GE\_GRAPH（详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)）环境变量，然后使用如下入口编译模型：

  - 使用ATC工具进行模型转换。ATC工具使用方法请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。
  - [编译Graph为离线模型](../compile_and_run_graph/compile_graph_to_offline_model.md)。
  - [编译并运行Graph](../compile_and_run_graph/compile_and_run_graph.md)。

## 结果验证

> [!NOTE]说明
>
>若一个动态shape模型中有可下沉的部分，框架内部会将模型拆分为动态调度和下沉调度（静态子图）两部分，其中下沉调度可能有多个小模型；因此在对一个动静混合模型做自定义流分配时，自定义Pass前后的dump图将会有多份：第一份对应根图，后面若干份对应模型中的静态模型，当希望通过dump图查看分配结果时，最好通过查看build图之前最后一次自定义流分配的dump图。

设置了dump环境变量后，程序执行完毕，会在当前路径生成ge\_onnx\*.pbtxt等图文件，用户可以获取如下两张图，然后使用Netron等可视化软件查看：

- ge\_onnx\__xxx_\_RunCustomPass\_BeforeAssignLogicStream\*.pbtxt：Pass执行前的图，\*代表具体的Pass名称，图结构请参见[图1](#fig1)。
- ge\_onnx\__xxx_\_RunCustomPass\_AfterAssignLogicStream\*.pbtxt：Pass执行后的图，\*代表具体的Pass名称，图结构为：

    ![图示](../figures/fusion_pass_image8.png)

    从图中可以看出算子1被分配了新的Stream ID。

用户也可以基于模型编译完成的图，并结合Profiling，查看最终的效果：从[图2](#fig2)查看，算子1前后产生了Send/Recv等流间同步算子；执行生成Profiling（[图3](#fig3)），可知算子1、2在两条流上并发执行。Profiling操作请参见[Profiling性能数据采集](../more_features/profiling_data_collection.md)。

**图 2**  编译完成的图<a id="fig2"></a>
![图示](../figures/compiled_graph.png "编译完成的图")

**图 3**  Profiling结果<a id="fig3"></a>
![图示](../figures/profiling_result.png "Profiling结果")
