# 专题

## UDF开发

### 开发流程

进行UDF开发的流程如下图所示。

**图1**  UDF开发流程
![](figures/UDF开发流程.png "UDF开发流程")

开发步骤详解如表1所示。

**表1**  UDF开发步骤详解

|步骤|描述|参考|
|--|--|--|
|环境准备|准备UDF开发及运行验证所依赖的开发环境与运行环境。|开发准备|
|工程创建|创建UDF工程。|工程创建|
|开发过程|用户继承meta_flow_func.h文件的MetaFlowFunc基类，重写Init和Proc两个函数。实现UDF的计算逻辑。|开发过程（调用MetaFlowFunc类）|
|工程编译|将用户自定义的函数cpp文件，编译成so文件。|工程编译|
|使用DataFlow API构建graph|使用DataFlow API，构建graph并执行。|使用DataFlow API构建graph|
|ST测试|系统测试（System Test），在真实的硬件环境中，验证UDF的正确性。|测试|

### 开发准备

#### 环境准备

- 进行UDF开发前，需要完成驱动固件及开发套件包Ascend-cann-toolkit的安装，详细操作请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)。
- 配置环境变量CANN软件包安装路径ASCEND\_INSTALL\_PATH，UDF编译时根据该环境变量寻找依赖的头文件和so，如果不设置，默认该环境变量取值为“/usr/local/Ascend”。配置示例如下。

    ```shell
    export ASCEND_INSTALL_PATH=CANN包安装路径
    ```

#### 工程创建

UDF工程分成如下几类，请根据使用场景进行创建。

- 通过UDF实现用户自定义功能：该场景下，用户不调用第三方依赖库，不调用已经实现的NN功能，完全通过编写UDF相关文件实现自定义功能。
- 通过调用NN实现自定义功能：该场景下，用户需要的功能可以通过已有的NN功能承载，用户通过UDF调用NN功能即可。
- 通过调用第三方依赖库实现自定义功能：该场景下，用户需要的功能在第三方依赖库中已存在，用户通过UDF调用第三方依赖库中相关功能即可。

您可以直接下载[工程样例](https://gitcode.com/cann/ge/tree/master/examples/dflow/udf_workspace)，并基于此进行开发。如下UDF工程目录示例包括了三种场景，请根据使用场景下载对应的样例，基于如下规则在对应目录下进行UDF交付件的开发。

```cpp
├── 01_udf_add
│ ├── CMakeLists.txt
│ └── add_flow_func.cpp // 单func接口定义的add函数
├── 02_udf_call_add_nn
│ ├── CMakeLists.txt
│ └── call_nn_flow_func.cpp // UDF调用tensorflow模型
├── 03_udf_add_multi_func
│ ├── CMakeLists.txt
│ └── add_flow_func.cpp // 多func接口定义add函数，可以和04样例配合使用
├── 04_control_func
│ ├── CMakeLists.txt
│ └── control_func.cpp // 根据输入激活03中定义的多func中的某一个，可以和03样例配合使用
└── README.md
```

### 开发过程（调用MetaFlowFunc类）

**简介**

在UDF开发过程中，用户可以调用MetaFlowFunc类进行自定义的单func处理函数的编写。也可以调用MetaMultiFunc类进行自定义的多func处理函数的编写。本小节介绍调用MetaFlowFunc类进行UDF的开发过程。

UDF的实现包括两部分：

- UDF实现文件：用户自定义函数功能的代码实现，文件后缀为.cpp。按场景分为通过UDF实现用户自定义功能和通过调用NN实现自定义功能。
- UDF注册：通过REGISTER\_FLOW\_FUNC宏将实现类声明为func name，注册到UDF框架中。

> [!NOTE]说明
>
>- 单P场景下，支持一次输入对应多次输出，或者一次输入对应一次输出，或者多次输入对应一次输出。
>- 2P场景下，仅支持一次输入对应一次输出。
>- 如下以实现add功能的函数为例，介绍如何进行UDF开发、编译、构图及验证。

**UDF实现文件（通过UDF实现自定义功能）**

用户需要在工程的“workspace/xx.cpp”文件中进行用户自定义函数的开发。如下以“add\_flow\_func.cpp”为例进行介绍。

**函数分析：**

该函数实现Add功能，支持float类型和int类型的Add功能。

**明确输入和输出：**

包含两个输入，一个输出。

**函数实现：**

用户继承meta\_flow\_func.h文件的MetaFlowFunc基类，重写Init和Proc两个函数。

如果有资源需要释放，需要在析构函数中处理。

```cpp
namespace FlowFunc {
class AddFlowFunc: public MetaFlowFunc{};
}
```

> [!NOTE]说明
>如果用户需要使用TPRT（Task Parallel Runtime）并发功能，如下的Init\(\)和Proc\(\)函数中的实现请参考[开发过程（调用MetaMultiFunc类）](#开发过程调用metamultifunc类)。

- Init\(\)：执行初始化动作，如变量初始化，获取属性等，在本例中需要获取在DataFlow构图时在FunctionPp设置的out\_type属性。示例如下。

    ```cpp
    int32_t Init() override
        {
            // 通过MetaContext类的GetAttr方法，获取在算子构图中设置的out_type属性，属性值保存在类的私有变量outDataType_中，用于把源类型转换成outDataType_类型。
            auto getRet = context_->GetAttr("out_type", outDataType_);
            if (getRet != FLOW_FUNC_SUCCESS) {
                FLOW_FUNC_RUN_LOG_ERROR("GetAttr dType not exist. ");
                return getRet;
            }
            FLOW_FUNC_RUN_LOG_INFO("Add flow func init success");
            return FLOW_FUNC_SUCCESS;
        }
    ```

- Proc\(\)：用户自定义的计算处理函数。UDF框架在接收到输入Tensor的数据后，会调用此方法。

    ```cpp
    int32_t Proc(const std::vector<std::shared_ptr<FlowMsg>> &inputMsgs) override
    {
        FLOW_FUNC_LOG_DEBUG("proc start");
        // 在Add方法下，只有两个输入
        if (inputMsgs.size() != 2) {
            // 返回flow_func_defines.h中定义的错误码
            FLOW_FUNC_LOG_ERROR("add must have 2 inputs, but %zu", inputMsgs.size());
            return FLOW_FUNC_ERR_PARAM_INVALID;
        }

        // invalid input
        auto inputFlowMsg1 = inputFlowMsgs[0];
        auto inputFlowMsg2 = inputFlowMsgs[1];
        FLOW_FUNC_LOG_INFO("inputFlowMsg1.RetCode=%d, inputFlowMsg2.RetCode=%d", inputFlowMsg1->GetRetCode(), inputFlowMsg2->GetRetCode());
        // 判断输入msg中的RetCode是否是0，RetCode是上步的处理结果，如果是0表示正常，如果非0则报错
        if (inputFlowMsg1->GetRetCode() != 0 || inputFlowMsg2->GetRetCode() != 0) {
            FLOW_FUNC_LOG_ERROR("invalid input");
            return -1;
        }
        MsgType MsgType1 = inputFlowMsg1->GetMsgType();
        MsgType MsgType2 = inputFlowMsg2->GetMsgType();
        // 判断输入msg中的消息类型是否是tensor类型
        if (MsgType1 == MsgType::MSG_TYPE_TENSOR_DATA && MsgType2 == MsgType::MSG_TYPE_TENSOR_DATA) {
            // 获取输入的Tensor
            auto inputTensor1 = inputFlowMsg1->GetTensor();
            auto inputTensor2 = inputFlowMsg2->GetTensor();
            auto inputShape1 = inputTensor1->GetShape();
            // 根据输入shape和输出数据的dataType信息，申请FlowFunc算子的输出flow msg
            auto outputMsg = context_->AllocTensorMsg(inputShape1, outDataType_);
            if (outputMsg == nullptr) {
                FLOW_FUNC_LOG_ERROR("allow tensor msg failed");
                return -1;
            }
            // 调用第三方库中的AddTensor接口，实现Add功能
            if (AddDepend::Instance().AddTensor(inputTensor1, inputTensor2, outputMsg, outDataType_) != 0) {
                FLOW_FUNC_LOG_ERROR("add tensor failed");
                return -1;
            }
            // 输出计算结果
            return context_->SetOutput(0, outputMsg);
        }
        // 输入消息不是Tensor类型，返回错误
        FLOW_FUNC_LOG_ERROR("MsgType is not Tensor.");
        return -1;
    }

    ```

**UDF实现文件（通过调用NN实现自定义功能）**

用户需要在工程的“workspace/xx.cpp”文件中进行用户自定义函数的开发。如下以“call\_nn\_flow\_func.cpp”为例进行介绍。

**函数分析：**

该函数实现Add功能。

**明确输入和输出：**

包含两个输入，一个输出。

**函数实现：**

用户继承meta\_flow\_func.h文件的MetaFlowFunc基类，重写Init和Proc两个函数。

```cpp
class CallNnFlowFunc: public MetaFlowFunc{}
```

- Init\(\)：执行初始化动作，如变量初始化，获取属性等，在本例中Init不需要做处理，直接返回SUCCESS。

    ```cpp
    int32_t Init() override
    {
        FLOW_FUNC_RUN_LOG_INFO("call nn flow func init success");
        return FLOW_FUNC_SUCCESS;
    }
    ```

- Proc\(\)：用户自定义的计算处理函数。UDF框架在接收到输入Tensor的数据后，会调用此方法。

    示例如下：

    ```cpp
    int32_t Proc(const std::vector<std::shared_ptr<FlowMsg>> &inputMsgs) override
    {
        std::vector<std::shared_ptr<FlowMsg>> outputMsgs;
        // 调用RunFlowModel执行自定义处理函数中调用的NN模型，该NN模型实现了Add功能,是用户通过IR构图实现的。该模型执行结果放在outputMsgs中。
        auto ret = context_->RunFlowModel(dependKey_.c_str(), inputMsgs, outputMsgs, 100000);
        if (ret != FLOW_FUNC_SUCCESS) {
            FLOW_FUNC_LOG_ERROR("Run flow model failed, ret=%d, dependKey=%s", ret, dependKey_.c_str());
            return ret;
        }
        // 将调用NN模型的输出结果outputMsgs通过SetOutput输出。
        for (size_t i = 0; i < outputMsgs.size(); ++i) {
            ret = context_->SetOutput(i, outputMsgs[i]);
            if (ret != FLOW_FUNC_SUCCESS) {
                FLOW_FUNC_LOG_ERROR("Set output failed, ret=%d, index=%zu", ret, i);
                return ret;
            }
        }
        return FLOW_FUNC_SUCCESS;
    }
    // 其中dependKey_是类CallNnFlowFunc定义的私有数据成员：
    private:
        std::string dependKey_{"invoke_graph"};

    ```

**UDF注册**

通过REGISTER\_FLOW\_FUNC宏将实现类声明为func name，注册到UDF框架中。

```cpp
REGISTER_FLOW_FUNC("call_nn", AddFlowFunc);
```

- call\_nn：用户自定义函数名，需要用户根据自己的函数功能进行定制。
- AddFlowFunc：类名，需要和“UDF实现文件（通过UDF实现自定义功能）”中的类名保持一致。

### 开发过程（调用MetaMultiFunc类）

**简介**

在UDF开发过程中，用户可以调用MetaFlowFunc类进行自定义的单func处理函数的编写。也可以调用MetaMultiFunc类进行自定义的多func处理函数的编写。本小节介绍调用MetaMultiFunc类进行UDF的开发过程。

UDF的实现包括两部分：

- UDF实现文件：用户自定义函数功能的代码实现，文件后缀为.cpp。按场景分为通过UDF实现用户自定义功能和通过调用NN实现自定义功能。
- UDF注册：通过FLOW\_FUNC\_REGISTRAR宏将实现类声明为func name，注册到UDF框架中。

> [!NOTE]说明
>
>- 单P场景下，支持一次输入对应多次输出，或者一次输入对应一次输出，或者多次输入对应一次输出。
>- 2P场景下，仅支持一次输入对应一次输出。
>- 如下以实现add功能的函数为例，介绍如何进行UDF开发、编译、构图及验证。

**UDF实现文件（通过UDF实现自定义功能）**

用户需要在工程的“workspace/xx.cpp”文件中进行用户自定义函数的开发。如下以“add\_flow\_func.cpp”为例进行介绍。

**函数分析：**

该函数实现Add功能，支持float类型和int类型的Add功能。第一个处理函数实现a+b的功能，第二个函数实现2a+b的功能。

**明确输入和输出：**

每个proc函数都包含两个输入，一个输出。两个proc处理函数的输入队列不共享，输出队列共享。

**注意事项：**

多func场景下多个处理函数会并发调度，用户需要考虑多线程并发问题，如果加锁可能会影响确定性调度。

**函数实现：**

用户继承meta\_multi\_func.h文件的MetaMultiFunc基类，重写Init函数和自定义的多func处理函数。

如果有资源需要释放，需要在析构函数中处理。

```cpp
namespace FlowFunc {
class AddFlowFunc: public MetaMultiFunc{};
}
```

- Init\(\)：执行初始化动作，如变量初始化，获取属性等。如果FlowFunc中不需要进行初始化操作，则不需要添加该函数。在本例中需要获取在DataFlow构图时在FunctionPp设置的out\_type属性。示例如下。

    ```cpp
    int32_t Init(const std::shared_ptr<MetaParams> &params) override
        {
            // 通过MetaParams类的GetAttr方法，获取在算子构图中设置的out_type属性，属性值保存在类的私有变量outDataType_中，用于把源类型转换成outDataType_类型。
            auto getRet = params->GetAttr("out_type", outDataType_);
            if (getRet != FLOW_FUNC_SUCCESS) {
                FLOW_FUNC_RUN_LOG_ERROR("GetAttr dType not exist. ");
                return getRet;
            }
            // 初始化多func处理函数共享变量。
            setOutputCount_ = 0;
            // 如果需要开启TPRT并发功能，需要增加如下一行内容。
            //(void)UdfTprt::Init(params);
            return 0;
        }
    ```

- template：Add实现方法的模板函数。

    ```cpp
        // Add1实现方法的模板函数
        template<typename srcT, typename dstT>
        void Add1(srcT *src1, srcT *src2, dstT *dst, size_t count)
        {
            for (size_t i = 0; i < count; ++i) {
                dst[i] = src1[i] + src2[i];
            }
            /* 如果想使能上面三行代码运算的TPRT并发，可以使用下面五行代码替换上面三行代码（如下示例使用的是ParallelFor，也可以使用Submit+Wait。）
            std::function<void(const int64_t index)> func = [src1, src2, dst]
               (const int64_t index) {
                dst[index] = src1[index] + src2[index];
            };
            UdfTprt::ParallelFor(0, count, func);
            */
        }
        // Add2实现方法的模板函数
        template<typename srcT, typename dstT>
        void Add2(srcT *src1, srcT *src2, dstT *dst, size_t count)
        {
            for (size_t i = 0; i < count; ++i) {
                dst[i] = 2 * src1[i] + src2[i];
            }
            /* 如果想使能上面三行代码运算的TPRT并发，可以使用下面七行代码替换上面三行代码（如下示例使用的是Submit+Wait，也可以使用ParallelFor。）
            UdfTprtTaskAttr attr;
            for (size_t i = 0; i < count; ++i) {
                UdfTprt::Submit([src1, src2, dst, i]() {
                    dst[i] = 2 * src1[i] + src2[i];
                }, {}, {}, attr);
            }
            UdfTprt::Wait();
            */
        }

        template<typename T>
        void Add1(T *src1, T *src2, void *dst, size_t count)
        {
            if (outDataType_ == TensorDataType::DT_FLOAT) {
                Add1(src1, src2, static_cast<float *>(dst), count);
            }
            if (outDataType_ == TensorDataType::DT_UINT32) {
                Add1(src1, src2, static_cast<uint32_t *>(dst), count);
            }
            else if (outDataType_ == TensorDataType::DT_INT64) {
                Add1(src1, src2, static_cast<int64_t *>(dst), count);
            }
        }
        template<typename T>
        void Add2(T *src1, T *src2, void *dst, size_t count)
        {
            if (outDataType_ == TensorDataType::DT_FLOAT) {
                Add2(src1, src2, static_cast<float *>(dst), count);
            }
            if (outDataType_ == TensorDataType::DT_UINT32) {
                Add2(src1, src2, static_cast<uint32_t *>(dst), count);
            }
            else if (outDataType_ == TensorDataType::DT_INT64) {
                Add2(src1, src2, static_cast<int64_t *>(dst), count);
            }
        }
    ```

- 多func处理函数：用户自定义的计算处理函数。UDF框架在接收到输入Tensor的数据后，会调用此方法。

    代码示例如下。

    ```cpp
    // 用户自定义的多func处理函数名："Proc1"。该函数名用户自行定义，入参和返回值需要和原型保持一致。
    int32_t Proc1(const std::shared_ptr<MetaRunContext> &runContext, const std::vector<std::shared_ptr<FlowMsg>> &inputFlowMsgs)
        {
            // 校验add操作是否是有两个输入
            if (inputFlowMsgs.size() != 2) {
                FLOW_FUNC_LOG_ERROR("add must have 2 inputs, but %zu", inputMsgs.size());
                return -1;
            }
            auto inputFlowMsg1 = inputFlowMsgs[0];
            auto inputFlowMsg2 = inputFlowMsgs[1];
            // 无效输入校验：校验输入消息的错误码是否非0
            if (inputFlowMsg1->GetRetCode() != 0 || inputFlowMsg2->GetRetCode() != 0) {
                FLOW_FUNC_LOG_ERROR("invalid input");
                return -1;
            }
            // 获取输入数据的消息类型
            MsgType MsgType1 = inputFlowMsg1->GetMsgType();
            MsgType MsgType2 = inputFlowMsg2->GetMsgType();
            // 校验输入数据是否是Tensor类型
            if (MsgType1 == MsgType::MSG_TYPE_TENSOR_DATA && MsgType2 == MsgType::MSG_TYPE_TENSOR_DATA) {
                // 从msg中获取Tensor
                auto inputTensor1 = inputFlowMsg1->GetTensor();
                auto inputTensor2 = inputFlowMsg2->GetTensor();
                // 从Tensor中获取data type
                auto inputDataType1 = inputTensor1->GetDataType();
                auto inputDataType2 = inputTensor2->GetDataType();
                // 校验两个输入的data type是否一致
                if (inputDataType1 != inputDataType2) {
                    FLOW_FUNC_LOG_ERROR("input type not be same");
                    return -1;
                }
                // 从Tensor中获取shape信息
                auto &inputShape1 = inputTensor1->GetShape();
                auto &inputShape2 = inputTensor2->GetShape();

                // 校验两个输入tensor的shape是否一致
                if (inputShape1 != inputShape2) {
                    FLOW_FUNC_LOG_ERROR("input shape not be same");
                    return -1;
                }
                // 申请输出的msg信息
                auto outputMsg = runContext->AllocTensorMsg(inputShape1, outDataType_);
                if (outputMsg == nullptr) {
                    FLOW_FUNC_LOG_ERROR("all tensor fail");
                    return -1;
                }
                auto outputTensor = outputMsg->GetTensor();
                // 从Tensor中获取tensor的数据大小
                auto dataSize1 = inputTensor1->GetDataSize();
                auto dataSize2 = inputTensor2->GetDataSize();

                if (dataSize1 == 0) {
                    return runContext->SetOutput(0, outputMsg);
                }
                // 从Tensor中获取tensor的数据指针
                auto inputData1 = inputTensor1->GetData();
                auto inputData2 = inputTensor2->GetData();
                auto outputData = outputTensor->GetData();
                // 根据不同的数据类型，执行不同的add操作
                switch (inputDataType1) {
                    case TensorDataType::DT_FLOAT:
                        Add1(static_cast<float *>(inputData1), static_cast<float *>(inputData2), outputData, dataSize1 / sizeof(float));
                        break;
                    case TensorDataType::DT_INT16:
                        Add1(static_cast<int16_t *>(inputData1), static_cast<int16_t *>(inputData2), outputData, dataSize1 / sizeof(int16_t));
                        break;
                    case TensorDataType::DT_UINT16:
                        Add1(static_cast<uint16_t *>(inputData1), static_cast<uint16_t *>(inputData2), outputData, dataSize1 / sizeof(uint16_t));
                        break;
                    case TensorDataType::DT_UINT32:
                        Add1(static_cast<uint32_t *>(inputData1), static_cast<uint32_t *>(inputData2), outputData, dataSize1 / sizeof(uint32_t));
                        break;
                    case TensorDataType::DT_INT8:
                        Add1(static_cast<int8_t *>(inputData1), static_cast<int8_t *>(inputData2), outputData, dataSize1 / sizeof(int8_t));
                        break;
                    case TensorDataType::DT_INT64:
                        Add1(static_cast<int64_t *>(inputData1), static_cast<int64_t *>(inputData2), outputData, dataSize1 / sizeof(int64_t));
                        break;
                    default:
                        // 不支持的data type，设置错误码输出。
                        outputMsg->SetRetCode(100);
                        break;
                }
                // 多个处理函数并发调度，用户需要自己加锁处理共享数据
                std::unique_lock<std::mutex> lock(countMutex_);
                setOutputCount_++;
                return runContext->SetOutput(0, outputMsg);
            }
            FLOW_FUNC_LOG_ERROR("MsgType is not Tensor.");
            return -1;
        }
        // 用户自定义的多func处理函数名："Proc2"。该函数名用户自行定义，入参和返回值需要和原型保持一致。
        int32_t Proc2(const std::shared_ptr<MetaRunContext> &runContext, const std::vector<std::shared_ptr<FlowMsg>> &inputFlowMsgs)
        {
            // 校验add操作是否是有两个输入
            if (inputFlowMsgs.size() != 2) {
                FLOW_FUNC_LOG_ERROR("add must have 2 inputs, but %zu", inputMsgs.size());
                return -1;
            }
            auto inputFlowMsg1 = inputFlowMsgs[0];
            auto inputFlowMsg2 = inputFlowMsgs[1];
            // 无效输入校验：校验输入消息的错误码是否非0
            if (inputFlowMsg1->GetRetCode() != 0 || inputFlowMsg2->GetRetCode() != 0) {
                FLOW_FUNC_LOG_ERROR("invalid input");
                return -1;
            }
            // 获取输入数据的消息类型
            MsgType MsgType1 = inputFlowMsg1->GetMsgType();
            MsgType MsgType2 = inputFlowMsg2->GetMsgType();
            // 校验输入数据是否是Tensor类型
            if (MsgType1 == MsgType::MSG_TYPE_TENSOR_DATA && MsgType2 == MsgType::MSG_TYPE_TENSOR_DATA) {
                // 从输入msg中获取Tensor
                auto inputTensor1 = inputFlowMsg1->GetTensor();
                auto inputTensor2 = inputFlowMsg2->GetTensor();
                // 从Tensor中获取data type
                auto inputDataType1 = inputTensor1->GetDataType();
                auto inputDataType2 = inputTensor2->GetDataType();

                // 校验两个输入的data type是否一致
                if (inputDataType1 != inputDataType2) {
                    FLOW_FUNC_LOG_ERROR("allow Tensor msg failed");
                    return -1;
                }
                // 从tensor中获取shape信息
                auto &inputShape1 = inputTensor1->GetShape();
                auto &inputShape2 = inputTensor2->GetShape();

                // 校验两个输入tensor的shape是否一致
                if (inputShape1 != inputShape2) {
                    FLOW_FUNC_LOG_ERROR("input shape not be same");
                    return -1;
                }
                // 申请输出的Tensor信息
                auto outputMsg = runContext->AllocTensorMsg(inputShape1, outDataType_);
                if (outputMsg == nullptr) {
                    FLOW_FUNC_LOG_ERROR("all tensor fail");
                    return -1;
                }
                auto outputTensor = outputMsg->GetTensor();
                // 从Tensor中获取Tensor的数据大小
                auto dataSize1 = inputTensor1->GetDataSize();
                auto dataSize2 = inputTensor2->GetDataSize();
                if (dataSize1 == 0) {
                    return runContext->SetOutput(0, outputMsg);
                }
                // 从tensor中获取Tensor的数据指针
                auto inputData1 = inputTensor1->GetData();
                auto inputData2 = inputTensor2->GetData();
                auto outputData = outputTensor->GetData();
                // 根据不同的数据类型，执行具体的add操作
                switch (inputDataType1) {
                    case TensorDataType::DT_FLOAT:
                        Add2(static_cast<float *>(inputData1), static_cast<float *>(inputData2), outputData, dataSize1 / sizeof(float));
                        break;
                    case TensorDataType::DT_INT16:
                        Add2(static_cast<int16_t *>(inputData1), static_cast<int16_t *>(inputData2), outputData, dataSize1 / sizeof(int16_t));
                        break;
                    case TensorDataType::DT_UINT16:
                        Add2(static_cast<uint16_t *>(inputData1), static_cast<uint16_t *>(inputData2), outputData, dataSize1 / sizeof(uint16_t));
                        break;
                    case TensorDataType::DT_UINT32:
                        Add2(static_cast<uint32_t *>(inputData1), static_cast<uint32_t *>(inputData2), outputData, dataSize1 / sizeof(uint32_t));
                        break;
                    case TensorDataType::DT_INT8:
                        Add2(static_cast<int8_t *>(inputData1), static_cast<int8_t *>(inputData2), outputData, dataSize1 / sizeof(int8_t));
                        break;
                    case TensorDataType::DT_INT64:
                        Add2(static_cast<int64_t *>(inputData1), static_cast<int64_t *>(inputData2), outputData, dataSize1 / sizeof(int64_t));
                        break;
                    default:
                        // not support
                        outputMsg->SetRetCode(100);
                        break;
                }
                // 多个处理函数并发调度，用户需要自己加锁处理共享数据
                std::unique_lock<std::mutex> lock(countMutex_);
                setOutputCount_++;
                return runContext->SetOutput(0, outputMsg);
            }
            FLOW_FUNC_LOG_ERROR("MsgType is not Tensor.");
            return -1;
        }

    private:
        TensorDataType outDataType_;
        std::mutex countMutex_;
        uint32_t setOutputCount_;
    ```

**UDF实现文件（通过调用NN实现自定义功能）**

用户需要在工程的“workspace/xx.cpp”文件中进行用户自定义函数的开发。如下以“call\_nn\_flow\_func.cpp”为例进行介绍。

**函数分析：**

该函数实现Add功能。

**明确输入和输出：**

包含两个输入，一个输出。

**函数实现：**

用户继承meta\_flow\_func.h文件的MetaFlowFunc基类，重写Init函数和自定义的处理函数。

```cpp
class CallNnFlowFunc: public MetaMultiFunc{}
```

- Init\(\)：执行初始化动作，如变量初始化，获取属性等，在本例中Init不需要做处理，因此不需要该函数。
- Add\(\)：用户自定义的计算处理函数。UDF框架在接收到输入tensor的数据后，会调用此方法。

    示例如下：

    ```cpp
    // 用户自定义的多func处理函数名："Add"。该函数名用户自行定义，入参和返回值需要和原型保持一致。
    int32_t Add(const std::shared_ptr<MetaRunContext> &runContext, const std::vector<std::shared_ptr<FlowMsg>> &inputFlowMsgs)
    {
        std::vector<std::shared_ptr<FlowMsg>> outputMsgs;
        // 使用runContext的RunFlowModel方法，执行自定义处理函数中调用的NN模型，该NN模型实现了Add功能,是用户通过IR构图实现的。该模型执行结果放在outputMsgs中。
        auto ret = runContext->RunFlowModel("invoke_graph", inputMsgs, outputMsgs, 100000);
        if (ret != FLOW_FUNC_SUCCESS) {
            return ret;
        }
        // 将调用NN模型的输出结果outputMsgs通过SetOutput输出。
        for (size_t i = 0; i < outputMsgs.size(); ++i) {
            ret = runContext->SetOutput(i, outputMsgs[i]);
            if (ret != FLOW_FUNC_SUCCESS) {
                return ret;
            }
        }
        return FLOW_FUNC_SUCCESS;
    }
    ```

**UDF注册**

通过FLOW\_FUNC\_REGISTRAR宏将实现类声明为func name，注册到UDF框架中。

```cpp
FLOW_FUNC_REGISTRAR(AddFlowFunc)
    .RegProcFunc("Proc1", &AddFlowFunc::Proc1)
    .RegProcFunc("Proc2", &AddFlowFunc::Proc2);
FLOW_FUNC_REGISTRAR(CallNnFlowFunc)
    .RegProcFunc("CallNn_Add", &CallNnFlowFunc::Add);
```

- Proc1：用户自定义多func处理函数名，需要和在AddFlowFunc中定义的处理函数名保持一致。
- Proc2：用户自定义多func处理函数名，需要和在AddFlowFunc中定义的处理函数名保持一致。
- Add：用户自定义函数名，需要和在CallNnFlowFunc中定义的处理函数名保持一致。
- AddFlowFunc：类名，需要和“UDF实现文件（通过UDF实现自定义功能）”中的类名保持一致。
- CallNnFlowFunc：类名，需要和“UDF实现文件（通过调用NN实现自定义功能）”中的类名保持一致。

### 工程编译

在udf\_workspace/01\_udf\_add目录下创建build和release目录：

编译完成后在workspace/udf\_add/release目录下生成编译成功的so。

1. 创建build目录。

    ```shell
    mkdir -p build
    ```

2. 创建release目录。

    ```shell
    mkdir -p release
    ```

3. 进入build目录。

    ```shell
    cd build
    ```

4. 配置环境变量，当前以非root用户安装Toolkit后的默认路径为例，请用户根据set\_env.sh的实际路径执行如下命令。

    ```shell
    source ${HOME}/Ascend/cann/set_env.sh
    ```

    上述环境变量配置只在当前窗口生效，用户可以按需将以上命令写入环境变量配置文件（如.bashrc文件）。

5. 工程编译
    - 非交叉编译场景（开发环境和运行环境的架构一致）下示例如下：编译x86\_64类型的so，指定target lib为libadd.so，编译工具为g++。

        ```shell
        cmake .. -DTOOLCHAIN=g++ -DRELEASE_DIR=../release -DRESOURCE_TYPE=X86 -DUDF_TARGET_LIB=add && make
        ```

        - -DTOOLCHAIN：编译工具名称。比如g++。
        - -DRELEASE\_DIR：编译完成后，目标文件存放路径。
        - -DRESOURCE\_TYPE：编译的资源类型，可以取值：X86或者Aarch或者Ascend。
            - X86：目标so需要部署在X86架构的Host时，配置为X86。
            - Aarch：目标so需要部署在Aarch的Host时，配置为Aarch。
            - Ascend：目标so需要部署在Device时，配置为Ascend。

        - -DUDF\_TARGET\_LIB：编译完成后，目标so的名称。比如，设置为add，实际生成libadd.so。

    - 交叉编译（开发环境是x86\_64，运行环境是aarch64）场景下，编译Ascend类型的so,指定target lib为libadd.so，编译工具为\$\{INSTALL\_DIR\}/toolkit/toolchain/hcc/bin/aarch64-target-linux-gnu-g++。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

        示例如下：

        ```shell
        cmake .. -DTOOLCHAIN=${INSTALL_DIR}/toolkit/toolchain/hcc/bin/aarch64-target-linux-gnu-g++ -DRELEASE_DIR=../release -DRESOURCE_TYPE=Ascend -DUDF_TARGET_LIB=add && make
        ```

### 使用DataFlow API构建graph

1. 构建FlowGraph。

    通过FlowNode和FunctionPp构建graph。示例如下：

    ```cpp
    // 构造DataFlow的FlowGraph图
    dflow::FlowGraph flow_graph("flow_graph");

    // 构造输入节点
    auto data0 = dflow::FlowData("Data0", 0);
    auto data1 = dflow::FlowData("Data1", 1);

    auto node0 = dflow::FlowNode("node0", 2, 1).SetInput(0, data0).SetInput(1, data1);
    // function_pp
    auto pp0 = dflow::FunctionPp("func_pp0")
                   .SetCompileConfig("./add_func.json")
                   .SetInitParam("out_type", ge::DT_UINT32);
    node0.AddPp(&pp0);
    ```

2. 运行graph。

    有如下两种方式。

    - Session run方式，示例如下：

        ```cpp
        // 创建session
        ge::Session* session1 = new Session(options);
        assert(session1 != NULL);
        ret = session1->AddGraph(graphId, graph);
        if (!ret) { return; }
        // 运行graph
        ret = session1->RunGraph(graphId, input, output);
        if (!ret) { return; }
        GEFinalize();
        return;
        ```

    - DataFlow方式，示例如下：

        ```cpp
        // DataFlow异步接口执行graph：
        ge::DataFlowInfo dataFlowInfo;
        dataFlowInfo.SetStartTime(0);
        dataFlowInfo.SetEndTime(5);
        std::vector<ge::Tensor> inputsData = {inputTensor, inputTensor};
        geRet = session->FeedDataFlowGraph(0, inputsData, dataFlowInfo, 3000);
        if (geRet != ge::SUCCESS)
        {
         std::cout << "TEST====Feed input data failed============." << std::endl;
         ge::GEFinalize();
         return -1;
        }
        // 获取输出
        std::vector<ge::Tensor> outputsData;
        geRet = session->FetchDataFlowGraph(0, outputsData, dataFlowInfo, 3000);
        if (geRet != ge::SUCCESS)
        {
         std::cout << "TEST====Fetch output data failed." << std::endl;
         ge::GEFinalize();
         return -1;
        }
        ```

### 测试

请使用开发好的UDF，构建单UDF的FlowGraph并编译运行，详细操作可以参考[通过FlowOperator构建FlowGraph](constructing_flowgraph_using_flowoperator.md)和[编译并运行FlowGraph](compiling_and_executing_flowgraph.md#编译并运行flowgraph)。

## 基于flow标记的图切分

**功能介绍**

前端表达仅支持IR表达，在同构引擎间，若希望进行Flow表达，可以通过"**\_flow\_attr**"设置Op或者SubGraphOp的flow属性，分别拆分成不同的Flow图，并分别进行图编译、部署和执行；对于子图，可以通过PartitionedCall算子来实现子图的嵌套；

"**\_flow\_attr**"支持两种设置方式：

- 使用SetAttr接口对Op设置flow属性，表示该Op的所有输入输出均为flow，与其相连接的Op均会被拆分成不同的FlowGraph。
- 使用SetInputAttr/SetOutputAttr接口对Op的输入或输出设置flow属性，表达该Op的指定输入/输出为flow，与其相连接的Op会被拆成不同的FlowGraph。

队列驱动场景会在异步Graph之间插入队列，队列的配置属性如下：

- "\_flow\_attr": BOOL类型属性，可以配置为true或false, true表示存在flow属性，false表示不存在flow属性。
- "\_flow\_attr\_depth": INT类型属性，指定了队列的深度，范围：大于等于2，若不配置，默认深度为128。
- "\_flow\_attr\_enqueue\_policy": STRING类型属性，指定的入队策略，仅支持"FIFO"和"OVERWRITE"，"FIFO"表示队列数据顺序入队，队列满的时候会等待dequeue，"OVERWRITE"表示入队不会等待，数据会循环覆盖，若不配置，默认策略为FIFO。
- 不支持对while/loop的V1控制算子结构进行FlowGraph拆分。

**使用方法**

IR构图示例：

```cpp
    auto data1 = op::Data("Data1").set_attr_index(0);
    auto data2 = op::Data("Data2").set_attr_index(1);
    auto data3 = op::Data("Data3").set_attr_index(2);
    auto data4 = op::Data("Data4").set_attr_index(3);

    float inputData1[2] = {1,1};
    Tensor input1(TensorDesc(ge::Shape({2}), FORMAT_ND, DT_FLOAT), (uint8_t *)inputData1, 2 * sizeof(float));
    input.push_back(input1);

    float inputData2[2] = {1,1};
    Tensor input2(TensorDesc(ge::Shape({2}), FORMAT_ND, DT_FLOAT), (uint8_t *)inputData2, 2 * sizeof(float));
    input.push_back(input2);

    float inputData3[2] = {1,1};
    Tensor input3(TensorDesc(ge::Shape({2}), FORMAT_ND, DT_FLOAT), (uint8_t *)inputData3, 2 * sizeof(float));
    input.push_back(input3);

    float inputData4[2] = {1,1};
    Tensor input4(TensorDesc(ge::Shape({2}), FORMAT_ND, DT_FLOAT), (uint8_t *)inputData4, 2 * sizeof(float));
    input.push_back(input4);

    auto OP_Reciprocal = op::Reciprocal("Reciprocal").set_input_x(data1);
    // 设置flow属性,op

    const std::string ATTR_NAME_FLOW_ATTR = "_flow_attr";
    const std::string ATTR_NAME_FLOW_ATTR_DEPTH= "_flow_attr_depth";
    const std::string ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY = "_flow_attr_enqueue_policy";

    OP_Reciprocal.SetAttr(ATTR_NAME_FLOW_ATTR.c_str(), true);
    AscendString policy("FIFO");
    OP_Reciprocal.SetAttr(ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy);

    auto OP_Square = op::Square("Square").set_input_x(data2);
    // 设置flow属性,op
    OP_Square.SetAttr(ATTR_NAME_FLOW_ATTR.c_str(), true);
    OP_Square.SetAttr(ATTR_NAME_FLOW_ATTR_DEPTH.c_str(), static_cast<int32_t>(16));
    AscendString policy1("");
    OP_Square.SetAttr(ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy1);

    auto OP_Add = op::Add("Add1").set_input_x1(OP_Reciprocal).set_input_x2(OP_Square);
    // 设置flow属性,op
    OP_Add.SetAttr(ATTR_NAME_FLOW_ATTR.c_str(), true);
    OP_Add.SetAttr(ATTR_NAME_FLOW_ATTR_DEPTH.c_str(), static_cast<int32_t>(16));
    AscendString policy2("OVERWRITE");
    OP_Add.SetAttr(ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy2);

    auto OP_Sub = op::Sub("Sub").set_input_x1(data3).set_input_x2(data4);

    auto OP_Mul = op::Mul("Mul").set_input_x1(OP_Add).set_input_x2(OP_Sub);
    auto partition1 = op::PartitionedCall("partitio_001")
 .create_dynamic_input_args(1)
 .set_dynamic_input_args(0, OP_Mul)
 .create_dynamic_output_output(1)
 .set_subgraph_builder_f(build_graph1);
    // 设置flow属性,PartitionedCall
    partition1.SetAttr(ATTR_NAME_FLOW_ATTR.c_str(), true);
    partition1.SetAttr(ATTR_NAME_FLOW_ATTR_DEPTH.c_str(), static_cast<int32_t>(16));
    AscendString policy3("FIFO");
    partition1.SetAttr(ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy3);

    auto result_output = op::Add("Add2").set_input_x1(partition1).set_input_x2(partition1);
    // 设置flow属性,input+output
    result_output.SetInputAttr(0,ATTR_NAME_FLOW_ATTR.c_str(), true);
    result_output.SetInputAttr(0,ATTR_NAME_FLOW_ATTR_DEPTH.c_str(), static_cast<int32_t>(32));
    AscendString policy4("OVERWRITE");
    result_output.SetInputAttr(0,ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy4);
    result_output.SetOutputAttr(0,ATTR_NAME_FLOW_ATTR.c_str(), true);
    result_output.SetOutputAttr(0,ATTR_NAME_FLOW_ATTR_DEPTH.c_str(), static_cast<int32_t>(32));
    AscendString policy5("OVERWRITE");
    result_output.SetOutputAttr(0,ATTR_NAME_FLOW_ATTR_ENQUEUE_POLICY.c_str(), policy5);

    std::vector<Operator> inputs{data1,data2,data3,data4};

    std::vector<Operator> outputs{result_output};
    graph.SetInputs(inputs).SetOutputs(outputs);
    ge::Session* session = new Session(options);
    session->AddGraph(graphId, graph);
    session->RunGraph(graphId, input, output);
    session->RemoveGraph(graphId);
    GEFinalize();
```

其中build\_graph定义为：

```cpp
Graph build_graph1(){
Graph graph("build_graph1");
std::vector<Tensor> input;
std::vector<Tensor> output;
auto data1 = op::Data("Data1").set_attr_index(0);
float inputData1[2] = {1,1};
Tensor input1(TensorDesc(ge::Shape({2}), FORMAT_ND, DT_INT32), (uint8_t *)inputData1, 2 * sizeof(float));
input.push_back(input1);
auto output_result=op::Cast("Cast_01").set_input_x(data1).set_attr_dst_type(DT_FLOAT);
std::vector<Operator> inputs{data1};
std::vector<Operator> outputs{output_result};
graph.SetInputs(inputs).SetOutputs(outputs);
return graph;
}
```

DataFlow构图示例：

```cpp
// DataFlow构图
auto node0 = dflow::FlowNode("node0", 2, 1)         // 创建FlowNode的FlowOperator实例
  .SetInput(0, data0)                     // 设置FlowNode第一个输入为data0
  .SetInput(1, data1);                    // 设置FlowNode第二个输入为data1
// 设置Flow属性
node0.SetAttr("_flow_attr", true);
node0.SetAttr("_flow_attr_depth", static_cast<int32_t>(108));
AscendString policy1("FIFO");
node0.SetAttr("_flow_attr_enqueue_policy", policy1);
```

## 指定DataFlow节点部署位置

**功能介绍**

支持使用配置文件指定DataFlow图节点部署位置。该功能可以实现多卡多实例。分摊计算量，提升性能。

UDF调用NN场景下，系统实现的部署策略如下。

- UDF部署在Host时，NN部署到第一个Device上。
- UDF部署在Device时，NN也部署在Device。

**使用约束**

- 指定部署时必须指定所有节点的部署位置。
- 根据AddGraph的options参数中的ge.experiment.data\_flow\_deploy\_info\_path指定部署配置文件，文件必须存在并且格式正确，具体格式要求请参见“配置文件格式”。
- 部署位置必须与节点实际可部署位置一致，比如UDF如果只支持X86，就不能指定部署到Device，同样如果节点只能部署到Device，就不能指定部署到Host。
- 同一个节点不能重复配置。
- 同一个节点不支持同时部署在Host和Device。

**使用方法**

使用示例如下。

```cpp
std::map<ge::AscendString, ge::AscendString> session_options = {};
std::shared_ptr<ge::Session> session = std::make_shared<ge::Session>(session_options);
const auto graph = CreateDataFlowGraph();
std::map<ge::AscendString, ge::AscendString> graph_options = {{"ge.experiment.data_flow_deploy_info_path", "./data_flow_deploy_info.json"}};
auto = session->AddGraph(0, graph, graph_options);
...
```

**配置文件格式**

典型配置文件data\_flow\_deploy\_info.json的格式要求示例如下。

```json
{

   "keep_logic_device_order": false,
   "batch_deploy_info": [
        {
            "flow_node_list": ["flowNode1", "flowNode2"],
            "logic_device_list": "0:0:-1:0"
        },
        {
            "flow_node_list": ["flowNode3"],
            "logic_device_list": "0:0:1:1"

        },
        {
            "flow_node_list": ["flowNode4"],
            "logic_device_list": "0:0:0:0,0:0:1:0",
            "invoke_list":[
            {
               "invoke_name":"invoked_flow_graph_name",
               "deploy_info_file":"./data_flow_invoke_flow_graph_deploy_info.json"
             },
            {
               "invoke_name":"invoked_flow_graph_name1",
               "logic_device_list": "0:0:0:0"
             }]
        },
        {
            "flow_node_list": ["flowNode5", "flowNode6"],
            "logic_device_list": "0:0:2~3:0~1"
        },
        {
            "flow_node_list": ["flowNode7", "flowNode8"],
            "logic_device_list": "0:0:0~1:0,0:0:2:1"
        }
    ]
}
```

**表1**  配置项说明

|配置项|配置说明|
|--|--|
|keep_logic_device_order|是否按照用户配置的device_list顺序部署实例。<br>取值如下：<br>true：是，在多实例场景下，按照用户配置的device_list，对Device进行排序部署。<br>false：否，在多实例场景下，按照框架内部实现逻辑对Device进行排序部署。<br>默认值：false。|
|**batch_deploy_info**|-|
|flow_node_list|FlowNode节点名列表，支持一个或者多个，多个时使用半角逗号分割。|
|logic_device_list|DataFlow图节点部署位置。<br>**格式为**：clusterid:serverid:deviceid:numaid（pgid/dieid），各字段含义如下：<br>- clusterid：集群的ID，当前固定为0。<br>- serverid：服务器节点的ID，请配置为numa_config.json中的node_id。<br>- deviceid：设备的逻辑编号。对应的是环境变量RESOURCE_CONFIG_PATH指定的numa_config文件中“node>item_list”字段的item顺序，从0开始。<br>- numaid（pgid/dieid）：单设备多计算单元的编号。<br>**典型场景如下**。<br>FunctionPp的heavy_load=true时，会部署到指定节点对应的Host节点。当部署到多个Device时，按照如下进行配置：<br>- 逐个配置：如"0:0:0:0,0:0:0:1"，表示该node会被多实例部署到"0:0:0:0"和"0:0:0:1"两个设备上。<br>- 按照设备范围配置：如"0:0:0~1:0~1"，其中的"~"表示范围，"~"前面必须小于等于后面，"0:0:0~1:0~1"表示该node会被多实例部署到"0:0:0:0"、"0:0:0:1"、"0:0:1:0"、"0:0:1:1"2个设备的2个计算单元。<br>当FlowNode为调用子FlowGraph的父节点时，则不能配置多实例。|
|invoke_list（可选，不配置的情况下，子图部署节点与父节点部署节点一致）|-|
|invoke_name|FlowNode节点嵌套的InvokedClosure的name，对应为AddInvokedClosure接口的参数：name。|
|deploy_info_file|通过该配置项指定DataFlow子图节点部署位置，当invoke_name对应的InvokedClosure为DataFlow子图，可配置DataFlow子图对应的部署位置文件，例如./data_flow_invoke_flow_graph_deploy_info.json，该文件中包含的字段和格式要求请参考data_flow_deploy_info.json。|
|logic_device_list|通过该配置项指定DataFlow子图或者AscendGraph子图节点部署位置，配置方式同batch_deploy_info中的logic_device_list。<br>对同一个子图节点，logic_device_list和deploy_info_file不可以同时配置，否则会报错。<br>对AscendGraph子图节点，logic_device_list和父节点的logic_device_list实例数需要相同，否则会报错。|

## DataFlow子图编译缓存

**功能介绍**

开启DataFlow编译缓存功能，当修改DataFlow单个节点的子图时，只增量编译修改的子图，其余子图使用缓存，从而降低编译耗时。

**使用约束**

- 该功能支持FlowGraph，不支持AscendGraph。
- 当前不支持带资源类算子的模型。
- 缓存不保证跨版本的兼容性，如果版本升级，需要清理缓存目录重新编译生成缓存。
- DataFlow子图发生变化后，原来的缓存文件不可用，用户需要手动删除缓存目录中的缓存文件，重新编译生成缓存文件。

**使用方法**

1. 缓存配置启用。

    配置示例如下。

    ```cpp
    std::map<ge::AscendString, ge::AscendString> session_options = {{"ge.graph_compiler_cache_dir", "./build_cache_dir"}};
    std::shared_ptr<ge::Session> session = std::make_shared<ge::Session>(session_options);
    const auto graph = CreateFlowGraph();
    std::map<ge::AscendString, ge::AscendString> graph_options = {{"ge.graph_key", "test_graph_001"}};
    auto = session->AddGraph(0, graph, graph_options);
    ...

    ```

     **表1**  参数解释

     |参数名|含义|
     |--|--|
     |ge.graph_compiler_cache_dir|图编译磁盘缓存目录，和ge.graph_key配合使用，ge.graph_compiler_cache_dir和ge.graph_key同时配置非空时，图编译磁盘缓存功能生效。<br>配置的缓存目录需要存在，否则会导致编译失败。<br>图发生变化后，原来的缓存文件不可用，用户需要手动删除缓存目录中的缓存文件，包括模型缓存文件、索引文件和变量格式文件，重新编译生成缓存文件。|
     |ge.graph_key|图唯一标识，建议取值只包含大小写字母（A-Z，a-z）、数字（0-9）、下划线（_）、中划线（-）并且长度不超过128。不满足条件时，系统会报错。|

    说明：对于ge.graph_compiler_cache_dir，如果用户需要减少缓存恢复时间，可以在该目录下增加配置文件"cache.conf"。示例如下。

    ```sh
    {
     "cache_manual_check":true,
     "cache_debug_mode":true
    }
    ```

    - cache_manual_check：是否开启缓存手工检查模式。配置为true时，表示当图发生变化再次编译时，需要手工删除DataFlow子图缓存文件夹下对应的子图缓存文件。

    - cache_debug_mode：是否开启缓存调试模式。配置为true时，不会生成整图缓存的文件（包括模型缓存文件、索引文件和变量格式文件，具体内容请参考"缓存文件生成规则"）。

    编译完成后，会在指定的目录下生成缓存文件。具体内容请参考“缓存文件生成规则”。

2. 图发生变化后，如果需要重新编译。请参考如下步骤。

   **说明:**
   <br>图变化包括：修改graphpp对应的graph或config.json、修改UDF的实现文件和修改部署信息。
   <br>如果ge.graph\_compiler\_cache\_dir配置了"cache.conf"，并且cache\_debug\_mode=true，则不需要手工删除图缓存的文件。

   a.（可选）手动删除整图缓存的文件（包括模型缓存文件、索引文件和变量格式文件，具体内容请参考“缓存文件生成规则”。

   b. 重新编译。

      编译完成后，会在指定的目录下生成缓存文件。具体内容请参考“缓存文件生成规则”。

**缓存文件生成规则**

生成文件包括：

- 模型缓存文件。
- 索引文件，便于用户通过graph\_key快速找到对应的缓存文件，索引文件内容示例如下：

    ```json
    {
        "cache_file_list":[
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117202307.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117202307.rdcpkt"
            },
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117203007.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117203007.rdcpkt"
            }
        ]
    }
    ```

- 变量格式文件，仅在图中存在变量时生成。用于框架匹配模型缓存文件，如果graph\_key对应的图内变量格式发生变更，则之前缓存的缓存文件将无法直接恢复使用，该场景下会重新触发编译流程重新生成缓存文件。
- 如果开启了权重外置功能，即options选项中配置了ge.externalWeight参数，并且设置为1，则在**ge.graph\_compiler\_cache\_dir**参数指定路径下还会生成weight目录，用于保存原始网络中的Const/Constant节点的权重信息。
- DataFlow子图缓存文件夹，仅在FlowGraph场景下生成。以graph\_key命名，存放DataFlow子图编译时产生的缓存文件。

文件名生成规则：

- 当ge.graph\_key配置值只包含大小写字母（A-Z，a-z）、数字（0-9）、下划线（\_）、中划线（-）并且长度不超过128时
  - 索引文件命名为：ge.graph\_key+“.idx”。
  - 模型缓存文件命名为：ge.graph\_key+当前时间+“.om”。
  - 变量格式文件命名为：ge.graph\_key+当前时间+“.rdcpkt”。

- 不满足上面条件时，系统会报错。

## DataFlow离线编译

**功能介绍**

DataFlow离线编译是指在开发环境编译，在运行环境上加载和部署，从而实现编译和建链解耦。

**使用方法**

1. 开启图编译缓存。示例如下。

    ```cpp
    std::map<ge::AscendString, ge::AscendString> session_options = {{"ge.graph_compiler_cache_dir", "./build_cache_dir"}};
    std::shared_ptr<ge::Session> session = std::make_shared<ge::Session>(session_options);
    const auto graph = CreateFlowGraph();
    std::map<ge::AscendString, ge::AscendString> graph_options = {{"ge.graph_key", "test_graph_001"}};
    auto = session->AddGraph(0, graph, graph_options);
    ...
    ```

2. 配置开发环境。

    通过环境变量RESOURCE\_CONFIG\_PATH配置目标执行环境的numa\_config.json文件。示例如下：

    ```shell
     export RESOURCE_CONFIG_PATH=numa_config.json//用于设置配置异构资源描述信息文件的存储路径。
    ```

    > [!NOTE]说明
    >**numa\_config.json**请参考[附录](appendices.md)。ipaddr字段可以使用任意IP。

    ```json
    {
                        "host":{
                             "resourceType": "X86",
                        .........
                        "devList":[
                          {
                          "ipaddr":"XX.XX.XX.XX",
                          "port":2509,
                          "deviceIdList":[0,1],
                          "resourceType":"Ascend",
                          "token":"OKIJBNHYGFVT7RGH",
                          "chip_count":2,
                          }
                       ],
    }
    ```

   **表1**  参数解释

   |参数名|含义|
   |--|--|
   |resourceType|可选，资源类型，可以取值为Ascend。需要与实际部署的运行环境中的resourceType保持一致。|
   |chip_count|必选，加速卡的数量。需要与实际部署的运行环境数量保持一致。如果不一致，系统做如下处理。<br>- chip_count>实际部署的运行环境数量时，部署阶段报错。<br>- chip_count<实际部署的运行环境数量且负载均衡未指定部署位置，则模型按照编译阶段负载均衡方案部署，运行环境多出的设备不进行部署。不配置时离线功能不生效。|

3. 图生成。
    1. 调用AddGraph接口时设置option  "ge.runFlag"="0"。
    2. 调用BuildGraph接口进行图编译，生成缓存文件。

        示例如下。

        ```cpp
        map<AscendString, AscendString> options = {{"ge.graph_compiler_cache_dir", "./build_cache_dir"}, {"ge.runFlag", "0"}};
        Session session(options);
        map<AscendString, AscendString> graph_options = {{"ge.graph_key", "graph_key1"}}
        session.AddGraph(1, g1, graph_options);
        std::vector<InputTensorInfo> inputs;
        auto ret = session.BuildGraph(1, inputs);
        // build接口可以替换成FeedDataFlowGraph接口
        auto ret = session.FeedDataFlowGraph(1, inputs, dataInfo);
        ```

    3. 离线部署。

        将开发环境中graph\_compiler\_cache\_dir路径下的模型缓存文件、索引文件和变量格式文件拷贝到运行环境的graph\_compiler\_cache\_dir路径。具体文件请参考“缓存文件生成规则”。约束条件如下：

        执行环境的numa\_config需要和编译时候保持一致，ipaddr字段除外。
