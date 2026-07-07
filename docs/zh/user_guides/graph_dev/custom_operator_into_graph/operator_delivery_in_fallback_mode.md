# 基于fallback形式下发算子

## 简介

Atlas 200I/500 A2 推理产品**不支持该特性**。

GE提供了两种常规的模型调度模式（下沉调度和Host调度）用于实现Host和Device之间的高效协同。

下沉调度模式通常适用于静态shape模型，由于输入tensor shape固定不变，可以在编译阶段完成内存编排和Tiling计算，因此可以将模型中的算子以整图形式下发到Device上，执行时，只需在Host侧下发一个模型执行的Task即可触发模型在Device上调度执行，从而提升模型调度性能。

Host调度模式通常适用于动态shape模型，由于输入tensor的shape不确定，必须在上一个算子完成shape推导后，才能确定下一个算子的输入shape等信息，因此整个模型无法下沉执行，通常需要将每个算子Kernel逐一下发到Device执行。

基于fallback形式下发算子属于Host调度模式的一种，用户可以在Host侧通过fallback函数执行算子，例如可直接调用aclnnXX单算子API执行算子。用户仅需在算子开发过程中调用Ascend C的[EnableFallBack](https://hiascend.com/document/redirect/CannCommunityAscendCApi)接口，即可自动生成fallback函数供GE自动回调，fallback函数的主要任务就是将GE的输入输出及属性转换为aclnnXX单算子API所需的参数格式，然后调用aclnnXX接口执行算子。

**图 1**  常规Host调度模式和fallback下发的对比
![图示](../figures/compare.png "常规Host调度模式和fallback下发的对比")

## 适用场景

- 不支持离线推理场景，仅适用于训练或在线推理场景。
- 通常建议针对动态shape模型中的多Kernel算子启用fallback下发。主要原因是：
  - 在静态shape场景下，aclnnXX接口包含Host操作，无法以整图形式下发，需要断图后以子图形式下发，这可能会对性能产生较大影响。
  - 在极少数情况下，算子开发者为了实现算子的极致性能优化，开发了多Kernel算子（如昇腾内置的Matmul和MC²等算子）。这类算子对应多个Kernel实现，在执行时需要Launch多个Kernel。由于Kernel的数量不确定，GE无法按照上述常规方式统一处理，因此需要通过fallback方式下发算子。
  - 对于单Kernel算子，启用fallback下发方式可能会导致性能下降。目前，基于Ascend C自定义算子工程自动生成的aclnnXX接口均为单Kernel算子API，因此不建议用户针对这些算子启用fallback下发。

## 实现原理

动态shape模型中多Kernel算子启用fallback下发的流程如下图所示。

**图 2**  动态shape模型中的多Kernel算子下发执行流程
![图示](../figures/dynamic_shape_multi_kernel_exec_flow.png "动态shape模型中的多Kernel算子下发执行流程")

fallback函数的主要任务是将GE的输入输出及属性转换为aclnn单算子API所需的参数格式，然后通过调用aclnnXX接口执行算子。以concat算子为例，fallback函数的格式为：

```c++
static graphStatus ConcatExecuteFunc(OpExecuteContext* host_api_ctx)
```

OpExecuteContext指针入参中主要包含计算fallback所需的信息，例如输入输出的shape和datatype等，具体请参考[《基础数据结构和接口》](https://hiascend.com/document/redirect/CannCommunitybasicopapi)\>"gert命名空间\>OpImplSpaceRegistryV2类"。

用户无需手动实现fallback函数，在算子原型注册过程中，只需调用EnableFallBack接口，系统将自动生成fallback函数并注册到GE。

## 使用指导

1. 算子开发阶段，用户在算子原型注册过程中调用Ascend C的EnableFallBack接口，自动生成fallback函数：

    ```c++
    class CustomOp : public OpDef {
    public:
        CustomOp(const char* name) : OpDef(name)
        {
            // 定义算子的输入/输出信息：包括是否必选、输入输出支持的DataType、Format
            this->Input("x").ParamType(REQUIRED).DataType({ge::DT_FLOAT}).Format({ge::FORMAT_ND});
            this->Input("y").ParamType(REQUIRED).DataType({ge::DT_FLOAT}).Format({ge::FORMAT_ND});
            this->Output("z").ParamType(REQUIRED).DataType({ge::DT_FLOAT}).Format({ge::FORMAT_ND});
            // 注册Shape推导函数
            this->SetInferShape(ge::InferShapeFunc);
            this->SetInferDataType(ge::InferDataTypeFunc);
            // 通过AddConfig注册算子支持的AI处理器型号
            this->AICore().AddConfig("ascendxxx");
            this->EnableFallBack();
        }
    };
    OP_ADD(CustomOp);
    ```

    当前fallback函数支持support\_aclnn和aclnn\_only两种调用模式，可通过ExtendCfgInfo接口的aclnnSupport.value参数进行配置，详细可参见[《Ascend C API》](https://hiascend.com/document/redirect/CannCommunityAscendCApi)。

    - support\_aclnn：此模式下，静态Shape场景中该算子通过模型下沉执行，动态Shape场景则在Host侧调用fallback函数下发算子。如果调用了EnableFallBack则默认采用该模式。
    - aclnn\_only：此模式下，动静态Shape场景中该算子均以fallback形式下发。不建议用户使用该模式，后续版本将被废弃。

2. 基于GE图模式进行训练或在线推理时，会自动回调fallback函数执行相关算子。

## 功能调试

support\_aclnn模式和aclnn\_only模式下的关键日志为：

```text
xxx, setting fallback attribute
```
