# ATC概述

## ATC工具介绍

介绍ATC工具的功能架构以及使用ATC工具过程中遇到的一些术语或者缩略语。

### ATC简介

昇腾张量编译器（Ascend Tensor Compiler，简称ATC）是异构计算架构CANN体系下的模型转换工具，它可以将开源框架的网络模型以及Ascend IR定义的单算子描述文件（JSON格式）转换为AI处理器支持的.om格式离线模型。其功能架构如[图1](#fig1)所示。

模型转换过程中，ATC会进行算子调度优化、权重数据重排、内存使用优化等具体操作，对原始的深度学习模型进行进一步的调优，从而满足部署场景下的高性能需求，使其能够高效执行在AI处理器上。

**图 1**  ATC工具功能架构<a id="fig1"></a>
![图示](../figures/atc_tool_func_architecture.png "ATC工具功能架构")

其中：

- 开源框架网络模型场景：
    1. 开源框架网络模型经过Parser解析后，转换为中间态IR Graph。
    2. 中间态IR经过图准备、图拆分、图优化、图编译等一系列操作后，转成适配AI处理器的离线模型（此处图指网络模型拓扑图）。
    3. 转换后的离线模型上传到板端环境，通过**模型加载接口加载模型文件，再调用模型执行接口**实现推理过程，详细流程请参见[《应用开发 \(C&C++\)》](https://hiascend.com/document/redirect/CannCommunityCppBaseinfer)中的“模型推理”章节。

- 单算子描述文件场景：

    Ascend IR定义的单算子描述文件（JSON格式）通过ATC工具进行单算子编译后，转成适配AI处理器的单算子离线模型，然后上传到板端环境，通过**单算子模型加载接口加载单算子模型文件**用于验证单算子功能，详细流程请参见[单算子模型执行](../../../api/graph_engine_api/c/acl/single_operator_model_execute.md)。

    关于单算子描述文件的详细配置说明请参见[单算子模型转换](../sinlgeop_model_convert/sinlgeop_model_convert.md)章节。

### 关键概念

- **GE**

    Graph Engine，图引擎，是计算图编译和运行的控制中心，提供图优化、图编译管理以及图执行控制等功能。GE通过统一的图开发接口提供多种AI框架的支持，不同AI框架的计算图可以实现到Ascend图的转换。原图优化时，GE内部会进行整图优化。

- **YUV420SP**

    有损图像颜色编码格式，常用为YUV420SP\_UV、YUV420SP\_VU两种格式。

- **数据排布格式（Format）**

    Format为数据的物理排布格式，定义了解读数据的维度，比如1D、2D、3D、4D、5D等。

- **NCHW和NHWC**

    在深度学习框架中，多维数据通过多维数组存储，比如卷积神经网络的特征图（Feature Map）通常用四维数组保存，即4D，4D格式解释如下：

  - N：Batch数量，例如图像的数目。
  - H：Height，特征图高度，即垂直高度方向的像素个数。
  - W：Width，特征图宽度，即水平宽度方向的像素个数。
  - C：Channels，特征图通道，例如彩色RGB图像的Channels为3。

    由于数据只能线性存储，因此这四个维度有对应的顺序。不同深度学习框架会按照不同的顺序存储特征图数据，比如Caffe，排列顺序为\[Batch, Channels, Height, Width\]，即NCHW；TensorFlow中，排列顺序为\[Batch, Height, Width, Channels\]，即NHWC。

    如[图2](#fig2)所示，以一张格式为RGB的图片为例，NCHW中，C排列在外层，每个通道内，像素紧挨在一起，实际存储的是“RRRRRRGGGGGGBBBBBB”，即同一通道的所有像素值顺序存储在一起；而NHWC中C排列在最内层，每个通道内，像素间隔挨在一起，实际存储的则是“RGBRGBRGBRGBRGBRGB”，即多个通道的同一位置的像素值顺序存储在一起。

    **图 2**  NCHW和NHWC<a id="fig2"></a>
    ![](../figures/nchw_and_nhwc.png "NCHW和NHWC")

- **NC1HWC0**

    AI处理器中，为了提高通用矩阵乘法（GEMM）运算数据块的访问效率，所有张量数据统一采用NC1HWC0的五维数据格式。

    其中C0与微架构强相关，是一个矩阵单元处理单边数据量，一个矩阵单元处理32B\*32B的数据，单边是32B；例如数据类型为float16（2字节）时，C0=32/2=16，数据类型为float32（4字节）时，C0=32/4=8。

    C1=\(C+C0-1\)/C0，如果结果不整除，向下取整。

    NHWC/NCHW -\> NC1HWC0的转换过程为：将数据在C维度进行分割，变成C1份NHWC0/NC0HW，再将C1份NHWC0/NC0HW在内存中连续排列成NC1HWC0，其格式转换示意图如下图所示。

    **图 3**  NC1HWC0<a id="fig3"></a>
    ![](../figures/nc1hwc0.png "NC1HWC0")

  - NHWC -\> NC1HWC0的转换公式如下：

        ```
        Tensor.reshape( [N, H, W, C1, C0]).transpose( [0, 3, 1, 2, 4] )
        ```

  - NCHW -\> NC1HWC0的转换公式如下：

        ```
        Tensor.reshape( [N, C1, C0, H, W]).transpose( [0, 1, 3, 4, 2] )
        ```

- **FRACTAL\_Z**

    FRACTAL\_Z是用于定义卷积权重的数据格式，由FT Matrix（FT：Filter，卷积核）变换得到。FRACTAL\_Z是送往Cube的最终数据格式，采用“C1HW,N1,N0,C0”的4维数据排布。

    数据有两层Tiling，如下图所示：

    ![](../figures/fractal_z_image.png)

    第一层与Cube的Size相关，数据按照列的方向连续（小n）；第二层与矩阵的Size相关，数据按照行的方向连续（大Z）。

    例如：HWCN = \(2, 2, 32, 32\)，将其变成FRACTAL\_Z\( C1HW, N1, N0, C0 \) = \(8, 2, 16, 16\)。

    HWCN变换FRACTAL\_Z的过程为：

    ```python
    Tensor.padding([ [0,0], [0,0], [0,(C0–C%C0)%C0], [0,(N0–N%N0)%N0] ]).reshape( [H, W, C1, C0, N1, N0]).transpose( [2, 0, 1, 4, 5, 3] ).reshape( [C1*H*W, N1, N0, C0])
    ```

    NCHW变换FRACTAL\_Z的过程为：

    ```python
    Tensor.padding([ [0,(N0–N%N0)%N0], [0,(C0–C%C0)%C0], [0,0], [0,0] ]).reshape( [N1, N0, C1, C0, H, W,]).transpose( [2, 4, 5, 0, 1, 3] ).reshape( [C1*H*W, N1, N0, C0])
    ```

- **FRACTAL\_NZ**

    FRACTAL\_NZ是分形格式，如Feature Map的数据存储，在cube单元计算时，输出矩阵的数据格式为NW1H1H0W0。整个矩阵被分为（H1\*W1）个分形，按照column major排布，形状如N字形；每个分形内部有（H0\*W0）个元素，按照row major排布，形状如z字形。考虑到数据排布格式，将NW1H1H0W0数据格式称为Nz（大N小z）格式。其中，H0,W0表示一个分形的大小，示意图如下所示：

    ![](../figures/nz_convert.png)

    ND –\> FRACTAL\_NZ的变换过程为：

    ```python
    (..., N, H, W )->pad->(..., N, H1*H0, W1*W0)->reshape->(..., N, H1, H0, W1, W0)->transpose->(..., N, W1, H1, H0, W0)
    ```

## 调用流程

ATC工具运行前需要准备环境和模型，本节给出ATC工具的运行流程以及和各组件的交互流程。

### 运行流程

运行流程如[图1](#fig3)所示。

**图 1**  运行流程<a id="fig3"></a>
![](../figures/runtime_flow.png "运行流程")

1. 使用ATC工具之前，请先在开发环境安装CANN软件包，获取相关路径下的ATC工具，然后设置环境变量，详细说明请参见[准备环境](environment_setup.md)。
2. 准备要进行转换的模型或单算子描述文件，并上传到开发环境。单算子描述文件相关配置请参见[单算子模型转换](../sinlgeop_model_convert/sinlgeop_model_convert.md)。
3. 使用ATC工具进行模型转换，模型转换过程中使用的参数请参见[参数说明](../CLI_options/README.md)。

### 模型转换交互流程

下面以开源框架网络模型转换为om离线模型为例，详细介绍模型转换过程中与周边模块的交互流程。

根据网络模型中算子计算单元的不同，分为AI Core算子和AI CPU算子：AI Core算子是指在AI处理器的核心计算单元上执行的算子，而AI CPU算子则是在AI CPU计算单元上执行的算子。

在AI Core算子、AI CPU算子的模型转换交互流程中，虽然都涉及图准备、图拆分、图优化、图编译等节点，但由于两者的计算单元不同，因此涉及交互的内部模块也有所不同，请参见下图。

关于算子类型、基本概念等详细介绍请参见[《Ascend C算子开发指南》](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)或[《TBE&AI CPU算子开发》](https://hiascend.com/document/redirect/CannCommunityOpdevWizard)（选择一种支持的方式即可）。如果用户使用的网络模型中有自定义算子，也请优先参见上述手册开发部署好自定义算子，模型转换时会优先去查找自定义算子库匹配模型文件中的算子；若匹配失败，则会去查找内置算子库。

模型转换过程中，若遇到AI CPU算子不支持某种数据类型导致编译失败的场景，可通过启用Cast算子自动插入特性快速将输入转换为算子支持的数据类型，从而实现网络的快速打通，详细流程请参见[开启AI CPU Cast算子自动插入特性](../references/enable_ai_cpu_cast_auto_insert.md)。

- AI Core算子模型转换交互流程

    **图 2**  AI Core算子模型转换交互流程
    ![](../figures/ai_core_op_model_conv_flow.png "AI-Core算子模型转换交互流程")

    1. 调用框架Parser功能，将主流框架的模型格式转换成CANN模型格式。
    2. 图准备阶段：该阶段会完成原图优化以及Infershape推导（设置算子输出的shape和dtype）等功能。
    3. 图拆分阶段：GE（Graph Engine，图引擎）根据引擎拆分多个子图。
    4. 图优化阶段：GE将拆分后的子图进行优化，优化时按照当前子图流程对AI Core算子进行预编译和UB（Unified Buffer）融合，然后根据算子信息库中算子信息找到算子实现将其编译成算子kernel（算子的\*.o与\*.json），最后将优化后子图返回给GE。优化后的子图合并为整图，再进行整图优化。

        <!-- npu="950" id1 -->
        Ascend 950PR/Ascend 950DT不支持UB融合。
        <!-- end id1 -->

    5. 图编译阶段：GE进行图编译，包含内存分配、流资源分配等，图编译完成之后生成适配AI处理器的离线模型文件（\*.om）。

- AI CPU算子模型转换交互流程

    <!-- npu="IPV350" id2 -->
    **IPV350不支持AI CPU相关特性。**
    <!-- end id2 -->

    **图 3**  AI CPU算子模型转换交互流程
    ![](../figures/ai_cpu_op_model_conv_flow.png "AI-CPU算子模型转换交互流程")

    1. 调用框架Parser功能，将主流框架的模型格式转换成CANN模型格式。
    2. 图准备阶段：该阶段会完成算子基本参数校验以及Infershape推导（设置算子输出的shape和dtype）等功能。

        另外，GE将整图下发给AI CPU Engine，AI CPU Engine读取算子信息库，匹配算子支持的format，并将format返回给GE。

    3. 图拆分阶段：GE根据引擎拆分多个子图。
    4. 图优化阶段：GE将拆分后的子图下发给AI CPU Engine，AI CPU Engine进行子图优化，并将优化后子图返回给GE。优化后的子图合并为整图，再进行整图优化。

    5. 图编译阶段：GE进行图编译，包含内存分配、流资源分配等，并向AI CPU Engine发送genTask请求，AI CPU Engine返回算子的taskinfo信息给GE，图编译完成之后生成适配AI处理器的离线模型文件（\*.om）。
