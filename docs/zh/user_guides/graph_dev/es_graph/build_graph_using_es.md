# 通过ES构建Graph

## 功能介绍

使用ES构建Graph，整体流程分为如下四个步骤：

![](../figures/cann_architecture_6.png)

其中：

1. 创建图构建器：初始化图构建器实例，用于提供构图所需的上下文、工作空间及构建相关方法。
2. 添加起始节点：起始节点指无输入依赖的节点，通常包括图的输入（如Data节点）和权重常量（如Const节点）。
3. 添加中间节点：中间节点为具有输入依赖的计算节点，通常由用户构图逻辑生成，并通过已有节点作为输入连接。
4. 设置图输出：明确图的输出节点，作为计算结果的终点。

构图过程中，主要涉及两个对象， 以C++为例：

- [Graph](../../../api/graph_engine_api/cpp/ge/Graph/overview.md)：表示最终构建完成的计算图，是构图的**目标**产物。
- [EsGraphBuilder](../../../api/graph_engine_api/cpp/ge/es/EsGraphBuilder/overview.md)：构图辅助类，提供节点添加、连接、属性设置等方法，并记录构图过程的中间状态。EsGraphBuilder仅在构图阶段存在，用于承载中间构建信息，是APP构图时直接操作的对象。在构图完成后，其内部状态被封装为Graph实例返回，EsGraphBuilder本身及其相关资源被释放。

## 构建Graph实例

以下是使用C、C++、Python三种语言实现ES构图的实例，以构造一个“两个输入求和”计算图为例：

- **C代码示例**：

    ```c
    #include "es_math_ops_c.h"                        // Add算子所在的聚合头文件，里面包含了所有算子的头文件合集以及图构建器等基础结构的头文件
    // 1. 创建图构建器（EsCGraphBuilder）
    EsCGraphBuilder *builder = EsCreateGraphBuilder("graph_name");
    // 2. 添加起始节点
    EsCTensorHolder *data0 = EsCreateGraphInput(builder, 0); // 添加第1个输入节点
    EsCTensorHolder *data1 = EsCreateInput(builder, 1); // 添加第2个输入节点
    // 3. 添加中间节点
    EsCTensorHolder *add = EsAdd(data0, data1);        // 添加加法计算节点（不再需要显式传入builder）
    // 4. 设置图输出
    EsSetOutput(add, 0);                                // 将add节点设置为图的第1个输出
    // 5. 完成构图，返回最终图对象
    EsGraph *graph = EsBuildGraphAndReset(builder); // 获取构建完成的图
    // 6.释放builder及其管理的过程资源
    EsDestroyGraphBuilder(builder);
    ```

    相关接口说明请参见[ES接口](../../../api/graph_engine_api/cpp/ge/es/es_interface.md)。

- **CPP代码示例**：

    ```c++
    #include "es_math_ops.h"                            // Add算子所在的聚合头文件，里面包含了所有算子的头文件合集以及图构建器等基础结构的头文件
    namespace ge {
    namespace es {
    // 1. 创建图构建器（EsGraphBuilder）
    EsGraphBuilder builder("graph_name");
    // 2. 添加2个输入节点
    std::vector<EsTensorHolder> inputs = builder.CreateInputs<2>();
    EsTensorHolder data0 = inputs[0];
    EsTensorHolder data1 = inputs[1];
    // 3. 添加中间节点，C++中，加减乘除等常用运算符被重载，可以直接使用
    EsTensorHolder add = data0 + data1;
    // 4. 设置图输出
    builder.SetOutput(add, 0);
    // 5. 完成构图，获取构造好的Graph对象，builder中的资源随析构而销毁
    std::unique_ptr<ge::Graph> graph = builder.BuildAndReset();
    }
    }
    ```

    相关接口说明请参见[ES接口](../../../api/graph_engine_api/cpp/ge/es/es_interface.md)。

- **Python代码示例：**

    使用Python代码构图之前，需要先安装[生成ES构图API（可选）](generate_es_graph_api_optional.md)中生成的whl包，安装示例如下：

    ```bash
    pip3 install {OUTPUT_PATH}/whl/es_math-1.0.0-py3-none-any.whl
    ```

    构图示例代码如下：

    ```python
    from ge.es import GraphBuilder
    from ge.es.math import Add
    # 1. 创建图构建器（EsBuilder）
    builder = GraphBuilder("graph_name")
    # 2. 添加 2 个输入节点
    data0, data1 = builder.create_inputs(2)
    # 3. 添加中间节点 Python中，加减乘除等常用运算符被重载，可以直接使用
    add = data0 + data1
    # 4. 设置图输出
    builder.set_output(add, 0)
    # 5. 完成构图，返回最终图对象
    graph = builder.build_and_reset()
    ```

    相关接口说明请参见[ES接口](../../../api/graph_engine_api/cpp/ge/es/es_interface.md)。

源码仓还给出了如下各种场景的ES构图样例，供用户参考：

**表 1**  ES构图sample

| 特性 | 获取链接 | 备注 |
| --- | --- | --- |
| 控制边场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/control_edge/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/control_edge/python) | 参见README执行相关操作。 |
| 控制算子场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/control_op/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/control_op/python) | 参见README执行相关操作。 |
| 动态输入场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/dynamic_input/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/dynamic_input/python) | 参见README执行相关操作。 |
| 动态输出场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/dynamic_output/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/dynamic_output/python) | 参见README执行相关操作。 |
| 设置普通属性的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/normal_attributes/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/normal_attributes/python) | 参见README执行相关操作。 |
| 普通输入的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/normal_input/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/normal_input/python) | 参见README执行相关操作。 |
| 操作符重载的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/operator_overload/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/operator_overload/python) | 参见README执行相关操作。 |
| 可选输入场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/optional_input/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/optional_input/python) | 参见README执行相关操作。 |
| 私有属性场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/private_attributes/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/private_attributes/python) | 参见README执行相关操作。 |
| transformer场景（部分片段）的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/transformer/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/transformer/python) | 参见README执行相关操作。 |
| 集合通信EP场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/hccl_ep/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/hccl_ep/python) | 说明：EP（Expert Parallel）图是指通过专家并行方式在多卡上运行的图结构。 |
| 集合通信TP场景的ES构图 | [C++代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/hccl_tp/cpp)<br>[Python代码示例](https://gitcode.com/cann/ge/tree/master/examples/es/hccl_tp/python) | 说明： TP（Tensor Parallel）图是指通过张量并行方式在多卡上运行的图结构。 |
| 自定义ES API并构图 | 单击[custom_es_api](https://gitcode.com/cann/ge/blob/master/examples/custom_es_api)获取样例，参见README执行相关操作。 | - |
