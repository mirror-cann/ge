# 执行动态Shape算子示例代码

本节介绍基于单算子模型执行的方式调用动态Shape算子的关键接口、示例代码。

## 基本原理

对于支持动态Shape的算子：

- 如果算子输出Shape明确时，该类算子执行的基本流程与固定Shape算子执行类似，接口调用流程请参见[单算子模型执行总体说明](single_operator_model_execute_overview.md)，执行固定Shape算子的示例代码请参见[执行固定Shape算子示例代码](static_shape_op_exec_sample_code.md)。
- 如果无法明确算子的输出Shape时，在调用aclopExecuteV2接口前，需用户调用aclopInferShape接口、aclGetTensorDescNumDims接口、aclGetTensorDescDimV2接口、aclGetTensorDescDimRange等接口，推导或预估算子的输出Shape，作为算子执行接口aclopExecuteV2的输入。

    aclopInferShape接口处，算子输入tensor数据的内存必须根据应用运行模式来确定，应用运行在Host时，此处需申请Host上的内存；应用运行在Device时，此处需申请Device上的内存。

## 示例代码

以下是关键步骤的代码示例，不能直接拷贝编译运行，仅供参考。调用接口后，需增加异常处理的分支，并记录报错日志、提示日志，此处不一一列举。

```c++
// ......

const char *opType;
int numInputs;
aclTensorDesc *inputDesc[2];
aclDataBuffer *inputs[2];
int numOutputs;
aclTensorDesc *outputDesc[1];
aclopAttr *attr;

aclError ret = aclopInferShape(opType, numInputs, inputDesc, inputs, numOutputs, outputDesc, attr);

std::vector<std::vector<int64_t>> tensorDims; // inferShape之后的输出tensor的Shape
// 循环算子的每一个输出，推导或预估Shape值：
for (int index = 0; index < numOutputs; ++index) {
    std::vector<int64_t> dimSize; // 表示执行算子时，输出的shape
    size_t dimNums = aclGetTensorDescNumDims(outputDesc[index]);
        // 表示动态Shape场景下维度个数未知，该场景预留
    if (dimNums == ACL_UNKNOWN_RANK)  {
        // 由用户预估最大Shape值max shape
        dimSize.push_back(max_shape);
    } else {
        for (size_t i = 0; i < dimNums; ++i) {
            int64_t dim;
            ret = aclGetTensorDescDimV2(outputDesc[index], i, &dim);

                        // 表示动态Shape场景下维度值是动态的
            if(dim == -1) {
                int64_t dimRange[2];
                                // 获取Shape范围，使用该范围中的Shape最大值来构造输出tensorDesc，作为aclopExecuteV2的输入
                ret = aclGetTensorDescDimRange(outputDesc[index], i, 2, dimRange);
                dim = dimRange[1];
            }
            dimSize.push_back(dim);
        }
    }
    tensorDims.push_back(dimSize);
}

// 构造算子输入tensorDesc和输入tensor，作为aclopExecuteV2的输入
aclTensorDesc *inputDescNew[2];
aclDataBuffer *inputsNew[2];
aclDataBuffer *outputsNew[1];
// 以上给出了执行算子时输出的Shape, 根据tensorDims中的dims构造输出tensorDesc（即outputDescNew参数值）, 用于调用aclopExecuteV2
ret = aclopExecuteV2(opType, numInputs, inputDescNew, inputsNew, numOutputs, outputDescNew, outputsNew, attr, stream);

// 针对上面用户预估Shape值以及使用Shape范围中的最大Shape的场景，在算子执行结束后，需增加下面的调用，获取准确的Shape：
// for 循环每一个输出的tensorDesc
std::vector<std::vector<int64_t>> outTensorDims; // 准确的输出tensorShape
for (int index = 0; index < numOutputs; ++index) {
    std::vector<int64_t> dimSize;
    int dimNums = aclGetTensorDescNumDims(outputDescNew[index]);
    for (int i = 0; i < dimNums; i++){
        int64_t dim;
        ret = aclGetTensorDescDimV2(outputDescNew[index], i, &dim);
        dimSize.push_back(dim);
    }
    outTensorDims.push_back(dimSize);
}
// ......
```
