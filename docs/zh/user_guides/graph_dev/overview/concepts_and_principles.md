# 概念原理

## Tensor/Node/Graph

Ascend IR（Ascend Intermediate Representation，昇腾中间表示）是AI处理器专用的抽象数据结构，用于表达计算流程。基于GE图引擎能力，PyTorch、TensorFlow、MindSpore、PaddlePaddle等主流AI框架的算法模型可以统一转换为使用Ascend IR表示的计算图，同时也支持用户自定义构建计算图，后续的编译加速优化均基于该计算图完成。

**图 1**  使用Ascend IR表示的计算图
![IR表示的计算图](../figures/ascend_ir_compute_graph.png "使用Ascend-IR表示的计算图")

Ascend IR主要包括张量（Tensor）、Node（Operator）、Graph三个维度的信息。

### Tensor

包括Tensor描述及数据两部分，Tensor描述包括了该Tensor的name、dtype、shape、format信息。

**表 1**  TensorDesc属性说明

|属性|定义|
|--|--|
|名称（name）|用于对Tensor进行索引，不同Tensor的name需要保持唯一。|
|形状（shape）|Tensor的形状，例如(10, )或者(1024, 1024)或者(2, 3, 4)等。如形状(3, 4)表示第一维有3个元素，第二维有4个元素，(3, 4)表示一个3行4列的矩阵数组。形式：(i1, i2, …, in)，其中i1到in均为正整数。|
|数据类型（dtype）|Tensor对象的数据类型。取值范围：float16, float32, int8, int16, int32, uint8, uint16, bfloat16, bool等。|
|数据排布格式（format）|数据的物理排布格式，详细请参见数据排布格式。|

### Node

Node（Operator）包括算子的name、type、输入、输出、属性等信息。

**表 2**  Operator属性说明

|属性|定义|
|--|--|
|名称（name）|算子的名称，用于标识图中的某个算子，同一图中算子的名称需要保持唯一。如图2所示Conv1、Pool1、Conv2都是图中的算子名称，其中Conv1与Conv2算子的类型为Convolution，表示分别做一次卷积运算。|
|类型（type）|图中每一个算子根据算子类型进行实现的匹配，相同类型的算子的实现逻辑相同。在一个图中同一类型的算子可能存在多个，例如上图中的Conv1算子与Conv2算子的类型都为Convolution。|
|输入（input）|算子的输入Tensor数据。|
|输出（output）|算子的输出Tensor数据。|
|属性（Attributes）|定义算子行为和功能，常见的算子属性包括轴（Axis）、权重（Weight）、偏差（Bias）。|

**图 2**  算子名称表示的计算图
![名称表示的计算图](../figures/op_name_compute_graph.png "算子名称表示的计算图")

### Graph

Graph包括name、算子列表、输入算子、输出算子等信息。

**表 3**  Graph属性说明

|属性|定义|
|--|--|
|名称（name）|图的名称，用于标识网络中的某个Graph，同一网络中Graph的名称需要保持唯一。|
|算子列表|图中所有的节点列表。|
|输入算子（input）|图的输入算子。|
|输出算子（output）|图的输出算子。|

## GE的工作原理

### 图构建

GE提供了两种构图方式：

- onnx/pb等模型图构建：用户通过ATC命令行工具或者C++语言的Parser接口，通过前端框架算子逐一映射成CANN算子，从而将框架模型文件（如\*.onnx和\*.pb等格式）解析成Ascend IR表示的计算图。

    **图 1**  Parser解析计算图
    ![Parser解析计算图](../figures/parser_parse_compute_graph.png "Parser解析计算图")

    ATC命令行工具详细说明请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/CannCommunityAtc)。

    使用Parser接口构建图请参见[使用Parser接口将原始模型解析为Graph](../construct_graph/parse_model_to_graph_using_parser.md)。

- 使用图引擎接口全新构建Graph：用户通过图引擎接口，将计算函数（算子）进行组合，构建成Ascend IR表示的计算图。下图展示了图构建的基本过程，详细过程可参考[使用图引擎接口全新构建Graph](../construct_graph/construct_graph_using_ge_api.md)。

    **图 2**  全新构建计算图
    ![全新构建计算图](../figures/build_compute_graph_new.png "全新构建计算图")

### 图编译与图优化

对于Ascend IR表示的计算图，GE适配底层硬件运行要求进行一系列的编译优化、编译生成om格式的离线模型，主要过程包括：

- 图准备：根据算子的输入张量信息、算子逻辑及算子属性等信息，提前推理出算子的输出张量描述，包括张量的形状、数据类型及数据排布格式等信息，算子构图准备阶段就可以为所有的张量静态分配内存，避免动态内存分配带来的开销，此过程通常称之为inferShapeAndType、inferFormat。同时进行与硬件无关的、算法层级的优化，包括但不限于常量折叠、冗余分支消除等。
- 图拆分：根据执行引擎（AI Core/AI CPU等）对算子分类，拆分形成不同的子图，方便后续在不同的硬件上进行针对性优化。
- 图优化：通过算子融合等图优化手段提升图的执行性能。可以进行和硬件无关的优化，比如将多个算子融合成1个或几个算子，节省计算时间；或者进行与硬件相关的优化，比如通过UB融合缩短数据在硬件内存上的搬运时间，从而提升执行效率。

    **Ascend 950PR/Ascend 950DT不支持UB融合。**

- 图编译：根据计算图分配运行资源，包括内存分配、stream资源分配等，并编译生成.om离线模型。

    ![CANN架构图](../figures/cann_architecture.png)

### 图加载与图执行

加载编译生成的离线模型文件，完成真实运行资源分配，并将stream/task下发到Device上执行，主要过程包括：

- 图加载：解析离线模型，分配真实的内存、创建stream运行资源。
- 图执行：拷贝输入数据；将stream/task下发到Device，由AI Core/AI CPU等执行对应的算子；Device计算完成后，返回给Host用户程序。

## 模型下沉调度

### 特性简介

在AI模型运行中，通常需要CPU和AI专用处理器（如NPU，又称AI处理器）协同工作。CPU所在位置称为主机端（Host），而NPU所在位置称为设备端（Device）。主机端擅长处理复杂的逻辑计算，而设备端擅长进行高并行计算。通过高效的计算调度机制，实现Host和Device之间的高效协同是提高AI模型性能的关键，能够显著提升异构系统资源的利用率。

Host CPU将模型中的算子依次下发到Device执行（如下图中的标号①所示），每一个算子在执行流上以1个或多个Task的形式存在，昇腾AI处理器依次拉取执行流上的Task执行。这种Host调度带来的问题是，需要Host和Device频繁交互，在实际的训练或推理场景中，模型会运行多次，每次运行都会触发Host把模型上的所有算子遍历下发一遍。

**图 1**  Host调度示意图
![Host调度示意图](../figures/host_scheduling_diagram.png "Host调度示意图")

对于输入tensor shape固定不变的静态shape的模型，在编译时即可确定所有算子的输入输出shape，结合昇腾内存复用算法，可完成模型级内存编排；静态shape模型在编译时还可提前完成所有算子的Tiling计算等Host侧计算。因此，GE提供了静态图下沉调度模式，让模型中的算子在加载阶段提前以整图的形式下发到Device上，在执行时，只需在Host侧下发一个模型执行的Task即可触发模型在Device上调度执行。相比于Host调度模式，下沉调度模式可大大降低Host侧调度开销，有效减少Host和Device之间的交互。

**图 2**  静态图下沉调度示意图
![静态图下沉调度示意图](../figures/static_graph_scheduling_diagram.png "静态图下沉调度示意图")

### 实现原理

模型下沉调度分为两个阶段，模型加载和模型执行。

- 模型加载：模型加载的具体动作和Host调度类似，即遍历图中的所有算子并将其整体下发至Device流上，区别在于下发到流上不立即执行。模型加载是一次性的动作，在首次模型执行时完成模型加载，如上图中的过程1所示。
- 模型执行：模型加载完成之后，可以像下发单算子Task一样，向执行流下发一个模型执行Task，昇腾AI处理器调度到该Task时（如上图 “执行流”中的E），执行模型中所有Task（如上图中的过程3）。如果需要多次运行模型，仅需多次下发模型执行Task，如上图中的过程2所示。

Host Bound调度和模型下沉调度的时序比较如下图所示，可以看出模型下沉执行的开始有一个模型下发的头开销，模型下沉执行E2E会有一个相对于Host调度的收益，模型的下沉头开销越小，性能提升幅度将越大。模型下沉调度Host/Device时序分析如下所示：

![时序图](../figures/time_series_analysis.png)

每次模型下发时，支持更新模型的Feature Map内存地址和输入输出内存地址。如果模型的Feature Map内存和模型输入输出内存发生了更新，则在模型下沉头开销（即上图中的m\_l\_t）中会完成模型内算子相关地址的刷新。
