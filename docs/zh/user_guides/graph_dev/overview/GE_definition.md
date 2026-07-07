# 什么是GE图引擎

## GE图引擎介绍

图引擎（Graph Engine，简称GE）是昇腾平台计算图编译和运行的控制中心，提供了图构建、图编译优化及图执行控制等功能。借助GE图引擎能力，PyTorch、TensorFlow、MindSpore、PaddlePaddle等主流AI框架的算法模型可以统一转换为使用Ascend IR（Ascend Intermediate Representation）表示的计算图（Ascend Graph），并通过GE的图编译加速技术，显著提升计算图在昇腾硬件上的执行效率。此外，GE还提供统一的图引擎接口，支持自定义图结构，帮助用户基于昇腾硬件快速部署神经网络业务。

**图 1**  GE逻辑架构图
![图示](../figures/ge_logical_architecture.png "GE逻辑架构图")

## GE图编译加速技术

当前主流的深度学习框架提供了Eager（Eager Execution，即时执行）模式和图模式的运行方式。Eager模式的特点是每个计算操作下发后立即执行，而图模式则是将所有计算操作构造成一张图，以图的形式下发执行。相较于单个计算操作依次下发的方式，图模式具备图的全局视角，能够更有效地简化和优化计算图操作，从而获得更优执行性能。

GE作为专门面向图模式的AI编译器，通过计算图优化、多流并行、内存复用和模型下沉等丰富的图编译加速技术，可有效减少Host与Device调度交互，减少模型内存占用，提升整图执行性能，加速模型在昇腾执行效率。关于GE关键技术的详细介绍，开发者可访问如下链接获取：

- [计算图优化](https://www.hiascend.com/zh/developer/techArticles/20240621-1)
- [多流并行](https://www.hiascend.com/zh/developer/techArticles/20240701-1)
- [内存复用](https://www.hiascend.com/zh/developer/techArticles/202407005-1)
- [模型下沉](https://www.hiascend.com/zh/developer/techArticles/20240715-1)
- [小shape算子计算优化技术](https://www.hiascend.com/zh/developer/techArticles/20250911-1)

## GE的开放能力

- 面向上层AI框架对接和业务部署场景：GE提供了统一的图引擎接口，支持对接上层基于图的开放框架，目前已适配PyTorch（TorchAir图模式）、TensorFlow、MindSpore、PaddlePaddle等主流AI框架；同时支持自定义图结构，帮助用户在昇腾硬件上高效部署神经网络业务。
- 面向AI模型整图编译和运行优化场景：GE开放了图编译优化和图执行能力，支持自定义图融合等功能，助力用户定制高性能图解决方案。

## GE使用入口

GE面向不同的用户场景，提供了不同的对接方案，便于用户更加便捷地使用GE图引擎能力。

- **onnx/pb等模型图模式执行**

    GE提供ATC命令行工具或者C++语言的Parser接口，支持将PyTorch和TensorFlow等框架导出的模型（\*.onnx/\*.pb/\*.air格式）转换为适配AI处理器的离线模型（\*.om格式），并基于图模式执行。详情请参见：

  - [使用ATC命令转换ONNX模型](quick_start.md#使用atc命令转换onnx模型)。
  - [使用Parser接口将原始模型解析为Graph](../construct_graph/parse_model_to_graph_using_parser.md)。

- **在PyTorch等AI框架内使用GE图模式**
  - 对于PyTorch、TensorFlow、MindSpore、PaddlePaddle等主流AI框架，目前已支持接入GE，用户仅需对训练代码进行少量的迁移适配即可使用GE图引擎能力，从而在AI处理器加速执行训练或在线推理任务。
    - PyTorch框架接入GE

        PyTorch网络支持通过Ascend Extension for PyTorch的TorchAir组件接入GE图引擎。TorchAir（Torch Ascend Intermediate Representation）是Ascend Extension for PyTorch（torch\_npu）中支持图模式能力的扩展库，对接PyTorch的[Dynamo特性](https://docs.pytorch.org/docs/main/user_guide/torch_compiler/torch.compiler_dynamo_overview.html)，可将PyTorch的[FX](https://pytorch.org/docs/main/fx.html)（Functionalization）图转换为Ascend IR（Intermediate Representation）表达的计算图，再通过GE进行计算图的编译优化等操作，下发到昇腾硬件执行。

        **图 2**  昇腾平台PyTorch图模式软件架构
        ![图示](../figures/pytorch_graph_arch.png "昇腾平台PyTorch图模式软件架构")

        TorchAir提供的昇腾编译后端能够作为参数传入PyTorch的torch.compile接口中，从而支持图模式执行，示例如下：

        ```C++
        # 先导入torch_npu，再导入torchair
        import torch
        import torch_npu
        import torchair

        # 定义模型Model
        class Model(torch.nn.Module):
            def __init__(self):
                super().__init__()
            def forward(self, x, y):
                return torch.add(x, y)

            # 实例化模型model
            model = Model()

            # 从TorchAir获取NPU提供的默认backend
            config = torchair.CompilerConfig()
            npu_backend = torchair.get_npu_backend(compiler_config=config)

            # 使用TorchAir的backend调用compile接口编译模型
            opt_model = torch.compile(model, backend=npu_backend)

            # 执行编译后的model
            x = torch.randn(2, 2)
            y = torch.randn(2, 2)
            opt_model(x, y)
        ```

        具体使用方法为：

        1. 将原始PyTorch网络迁移到昇腾平台，详细迁移过程请参见[《PyTorch 训练模型迁移调优指南》](https://hiascend.com/document/redirect/CannCommunityPtQuick)。
        2. 通过TorchAir实现图模式推理，详细配置方法具体请参见《PyTorch图模式使用指南\(TorchAir\)》。

    - TensorFlow框架接入GE

        TensorFlow 1.15网络为图模式执行方式，支持通过TF Adapter接入GE图引擎，将前端TensorFlow模型转换为Ascend IR表达的计算图，再通过GE进行计算图的编译优化等操作，下发到昇腾硬件执行。TensorFlow 2.6.5网络的默认执行方式为Eager模式（即单算子模式），即每个计算操作下发后立即执行并返回。TensorFlow 2.6.5提供了tf.function装饰器，用于将python函数中调用的TF2运算封装成graph执行，获取性能收益。当前针对TensorFlow 2.6.5，昇腾平台仅支持tf.function修饰的函数算子在昇腾硬件加速执行，即仅支持图模式执行。

        **图 3**  昇腾平台TensorFlow图模式软件架构
        ![图示](../figures/tensorflow_graph_arch.png "昇腾平台TensorFlow图模式软件架构")

        将原始TensorFlow 1.15网络迁移到昇腾平台执行的详细使用方法请参见《TensorFlow 1.15模型迁移》。

        将原始TensorFlow 2.6.5网络迁移到昇腾平台执行的详细使用方法请参见《TensorFlow 2.6.5模型迁移》。

    - MindSpore框架接入GE

        MindSpore网络原生支持GE图引擎，用户可以直接基于MindSpore框架使用GE图引擎能力，详细使用方法请参见[https://www.mindspore.cn/](https://www.mindspore.cn/)。

    - 飞桨（PaddlePaddle）框架接入GE

        详细使用方法请参见[《PaddlePaddle教程文档》](https://www.paddlepaddle.org.cn/documentation/docs/zh/develop/hardware_support/npu/install_cn.html)。

  - 对于其他三方框架，GE提供了统一的C++图引擎接口支持用户自行完成适配动作。详情请参见[使用图引擎接口全新构建Graph](../construct_graph/construct_graph_using_ge_api.md)。

- **使用图引擎接口全新构建网络**
  - GE提供了统一的C++图引擎接口支持用户自定义图结构，并在AI处理器基于图引擎执行。详情请参见：[使用图引擎接口全新构建Graph](../construct_graph/construct_graph_using_ge_api.md)。
  - GE提供了一套函数风格的极简构图API，可以使用更少的代码量来构图，提升构图易用性的同时兼顾了一定的防呆能力。详情请参见：[ES构图](../es_graph/README.md)。
