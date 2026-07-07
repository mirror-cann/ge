# 单算子模型执行总体说明

**须知：本章中的单算子模型执行接口后续版本会废弃，替换方案请参见[废弃接口替换方案](deprecated_APIs_replacement_scheme.md)。**

**单算子模型执行是指基于图IR（Intermediate Representation）执行算子**，先使用昇腾张量编译器（Ascend Tensor Compiler，简称ATC）将Ascend IR定义的单算子描述文件编译成算子om模型文件，再调用acl接口加载算子模型（例如[aclopSetModelDir](aclopSetModelDir.md)接口），最后调用acl接口执行算子（例如[aclopExecuteV2](aclopExecuteV2.md)接口）。

**图 1**  单算子模型执行接口调用流程
![](figures/单算子模型执行接口调用流程.png "单算子模型执行接口调用流程")

关键接口的说明如下（其中aclrt开头的接口是Runtime组件提供的接口）：

1. **编译算子**。

    根据算子编译的方式，可分为以下两种：

    - 编译算子后，算子相关数据保存在\*.om模型文件中

        该种方式下编译算子，需使用ATC工具，详细描述请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)，将单算子定义文件（\*.json）编译成适配AI处理器的离线模型（\*.om文件）。

        编译算子后，依次进行后续的2、3、4、5、6、7步。

    - 编译算子后，算子相关数据保存在内存中

        该种方式下编译算子，需调用acl接口，根据不同场景调用不同的接口：

        - 对于同一个算子，编译一次，多次执行的场景，建议调用aclopCompile接口编译算子。编译算子后，依次进行后续的3、4、5、6、7步。
        - 对于编译算子、执行算子次数相同的场景，建议先执行第3步，再调用aclopCompileAndExecute接口编译算子。编译算子后，再依次进行后续的6、7步。

2. **加载算子模型文件**。

    支持以下2种方式中的一种加载单算子模型文件：

    - 调用aclopSetModelDir接口，设置加载模型文件的目录，目录下存放单算子模型文件（\*.om文件）。
    - 调用aclopLoad接口，从内存中加载单算子模型数据，由用户管理内存。单算子模型数据是指“单算子编译成\*.om文件后，再将om文件读取到内存中”的数据。

3. 调用aclrtMalloc接口**申请Device上的内存**，存放执行算子的输入、输出数据。

    如果需要将Host上数据传输到Device，则需要调用aclrtMemcpy接口（同步接口）或aclrtMemcpyAsync接口（异步接口）通过内存复制的方式实现数据传输。

4. 动态Shape场景，如果无法明确算子的输出Shape时，在执行算子前，还需**推导或预估算子的输出Shape**。

    需用户调用aclopInferShape接口、aclGetTensorDescNumDims接口、aclGetTensorDescDimV2接口、aclGetTensorDescDimRange等接口，推导或预估算子的输出Shape，作为算子执行接口aclopExecuteV2的输入。

5. **执行算子**。

    - 对于被封装成acl接口的算子，包括GEMM算子（参见CBLAS接口）、Cast算子，目前支持以下两种执行方式：
        - 不以handle方式执行算子，接口名称中不包含“Handle”关键字，例如，调用aclblasGemmEx接口（封装GEMM算子）、aclopCast接口（封装Cast算子）等执行算子。
        - 以handle方式执行算子，接口名称中包含“Handle”关键字，例如，调用aclblasCreateHandleForGemmEx接口、aclopCreateHandleForCast接口等创建handle后，还需要调用aclopExecWithHandle接口执行算子。

    - 对于未被封装成acl接口的算子，目前支持以下两种执行方式：
        - 不以handle方式执行算子，调用aclopExecuteV2接口执行算子。
        - 以handle方式执行算子，调用aclopCreateHandle接口创建handle，再调用aclopExecWithHandle接口执行算子。

    >**说明：**
    >不以handle方式执行算子时，每次执行算子时，系统内部都会根据算子描述信息匹配内存中的模型。
    >以handle方式执行算子时，系统内部将算子描述信息匹配到内存中的模型，并缓存在handle中，每次执行算子时，无需重复匹配算子与模型，因此在涉及多次执行同一个算子时，效率更高，但该方式不支持动态Shape算子，且handle使用结束后，需调用aclopDestroyHandle接口释放。

6. 调用aclrtSynchronizeStream接口**阻塞应用运行**，直到指定Stream中的所有任务都完成。
7. 调用aclrtFree接口**释放内存**。

    如果需要将Device上的算子执行结果数据传输到Host，则需要调用aclrtMemcpy接口（同步接口）或aclrtMemcpyAsync接口（异步接口）通过内存复制的方式实现数据传输，然后再释放内存。
