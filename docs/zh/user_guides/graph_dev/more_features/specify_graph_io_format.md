# 指定Graph输入输出的内部格式

## 功能介绍

在了解本章节特性之前，需要先了解什么是原始格式和内部格式（或称运行时格式）：

原始格式是指未经任何转换的原始图像格式，对应脚本上使用的格式，常见的如NCHW、NHWC等；而为了确保算子在不同硬件规格上表现出最高效率，内部格式应运而生（对应Device上计算使用的格式），常用的有NC1HWC0、FRACTAL\_NZ、FRACTAL\_Z等。例如，脚本开发者模型中使用了NCHW格式的Tensor，经过图编译框架优化后，该Tensor可能被转换为运行时格式NC1HWC0。关于格式的详细介绍请参见[《Ascend C算子开发指南》](https://hiascend.com/document/redirect/CannCommunityopdevAscendC)中的“编程指南 \> 概念原理和术语 \> 神经网络和算子\> 数据排布格式”。

将原始格式转换为运行时格式，相对于原始脚本来说是一种额外开销，因此提供本章节的功能，支持用户指定模型输入输出的内部格式，从而减少Tensor在图边界传递过程中产生的额外格式转换开销，获取更大的性能收益。

该特性涉及的主要接口为：

![图示](../figures/feature_intro_12.png)

用户创建Graph实例后，对Data节点的输出TensorDesc，或模型输出的TensorDesc调用SetStorageFormat接口，设置运行时格式，调用SetExpandDimsRule接口，设置补维规则，然后在Graph中设置输入算子、输出算子，完成Graph构建。

**使用约束：**

- 设置Graph输入的内部格式，当前仅支持Data节点和RefData节点，其他类型节点暂不支持。
- 该特性仅支持[编译并运行Graph](../compile_and_run_graph/compile_and_run_graph.md)场景，暂不支持[编译Graph为离线模型](../compile_and_run_graph/compile_graph_to_offline_model.md)场景。
- 当原始shape维度小于4的时候，转换为内部格式需要同时指定补维规则，不同的补维规则转为内部格式后数据排布也会不同。

## 使用方法

下面以构造一个Graph为例，详细介绍如何设置输入RefData节点以及输出节点的内部格式，构造的Graph样例如下：

![](../figures/specify_internal_format.png)

样例代码如下：

```c++
// 1.构造整图输入RefData节点并设置运行时格式
//  1.1 构造RefData的tensor desc
std::vector<int64_t> weight_shape = {4,50};
TensorDesc weight_desc = TensorDesc(ge::Shape(weight_shape), FORMAT_NHWC, DT_FLOAT);
weight_desc.SetStorageFormat(FORMAT_NC1HWC0); // 设置tensor desc的运行时格式
weight_desc.SetExpandDimsRule(AscendString("NC")); //设置tensor desc的补维规则
//  1.2 构造RefData节点，并设置其输入输出tensor desc
auto weight = op::RefData("weight").set_attr_index(1);
//  1.3 为RefData节点设置输入输出tensor desc
weight.update_input_desc_x(refdata_01_desc);
weight.update_output_desc_y(refdata_01_desc);

// 2.构造图中其他节点
auto fm_data = op::Data("data").set_attr_index(0);

// 3.构造整图输出Conv2D节点并设置输出的运行时格式
//  3.1 构造Conv2D的输出tensor desc
std::vector<int64_t> conv2d_out_shape = {4,50};
TensorDesc conv2d_out_desc = TensorDesc(ge::Shape(conv2d_out_shape), FORMAT_NHWC, DT_FLOAT);
conv2d_out_desc.SetStorageFormat(FORMAT_NC1HWC0); // 设置tensor desc的运行时格式
conv2d_out_desc.SetExpandDimsRule(AscendString("NC")); //设置tensor desc的补维规则
//  3.2 构造Conv2D节点
auto conv2d = op::Conv2D("conv2d").set_input_x(fm_data).set_input_filter(weight);
//  3.3 为Conv2D节点设置输出tensor desc
conv2d.update_output_desc_y(conv2d_out_desc);

// 4.构造整图并设置整网输入输出
ge::Graph graph("demo_graph");
graph.SetInputs({fm_data, weight}).SetOutputs({{conv2d, 0}});
```
