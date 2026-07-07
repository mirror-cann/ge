# 调用CBLAS接口执行算子示例代码

本节介绍基于单算子模型执行的方式调用CBLAS算子的关键接口、示例代码。

## 基本原理

接口调用流程，请参见[单算子模型执行总体说明](single_operator_model_execute_overview.md)。

**目前，GEMM算子（用于矩阵-向量乘、矩阵-矩阵乘）已被封装成aclblas接口**，目前支持以下两种执行方式：

- 不以handle方式执行算子，接口名称中不包含“Handle”关键字，例如，调用aclblasGemmEx接口（封装GEMM算子）执行算子。
- 以handle方式执行算子，接口名称中包含“Handle”关键字，例如，调用aclblasCreateHandleForGemmEx接口、创建handle后，还需要调用aclopExecWithHandle接口执行算子。

>**说明：**
>不以handle方式执行算子时，每次执行算子时，系统内部都会根据算子描述信息匹配内存中的模型。
>以handle方式执行算子时，系统内部将算子描述信息匹配到内存中的模型，并缓存在handle中，每次执行算子时，无需重复匹配算子与模型，因此在涉及多次执行同一个算子时，效率更高，但该方式不支持动态Shape算子，且handle使用结束后，需调用aclopDestroyHandle接口释放。

## 示例代码

本章以aclblasGemmEx接口为例，该接口封装的是GEMM算子，该接口中矩阵乘的计算公式为：C = αAB + βC，表示矩阵A和矩阵B相乘后得到矩阵C，α和β表示乘积的系数。

调用CBLAS接口（封装GEMM算子）分为以下几步：

1. 准备GEMM算子的模型文件。
    1. 构造GEMM算子的描述文件（\*.json文件，描述输入输出Tensor描述、算子属性等）。

        GEMM算子的描述文件示例如下：

        ```bash
        [
        {
          "op": "GEMM",
          "input_desc": [
            {
              "format": "ND",
              "shape": [16, 16],
              "type": "float16"
            },
            {
              "format": "ND",
              "shape": [16, 16],
              "type": "float16"
            },
            {
              "format": "ND",
              "shape": [16, 16],
              "type": "float16"
            },
            {
              "format": "ND",
              "shape": [],
              "type": "float16"
            },
            {
              "format": "ND",
              "shape": [],
              "type": "float16"
            }
          ],
          "output_desc": [
            {
              "format": "ND",
              "shape": [16, 16],
              "type": "float16"
            }
          ],
          "attr": [
          {
            "name": "transpose_a",
            "type": "bool",
            "value": false
          },
          {
            "name": "transpose_b",
            "type": "bool",
            "value": false
            }
          ]
        }
        ]
        ```

    2. 借助ATC工具，将该算子描述文件编译成单算子模型文件（\*.om文件），再分别调用acl接口加载om模型文件、执行算子。

        ATC工具的命令示例如下：

        ```bash
        atc --singleop=$HOME/singleop/gemm.json --output=$HOME/singleop/out/op_model --soc_version=<soc_version>
        ```

        关键参数解释如下，详细参数取值及约束说明请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)：

        - --singleop：单算子描述文件（json格式）的路径。
        - --output：存放单算子模型文件的目录。
        - --soc\_version：AI处理器的版本。_<soc\_version\>_请根据实际情况替换。

2. 编写调用CBLAS的代码逻辑。

    以下是关键步骤的代码示例，不能直接拷贝编译运行，仅供参考。调用接口后，需增加异常处理的分支，并记录报错日志、提示日志，此处不一一列举。

    您可以单击[gemm](https://gitee.com/ascend/samples/tree/master/cplusplus/level1_single_api/1_acl/4_blas/gemm)获取样例。

    ```c++
    // 1.初始化
    aclRet = aclInit(nullptr);

    // 2.运行时资源申请（使用默认Context、默认Stream，默认Stream在作为其它接口入参时，可传空指针）
    aclRet = aclrtSetDevice(0);
    // 获取软件栈的运行模式，不同运行模式影响后续的接口调用流程（例如是否进行数据传输等）
    aclrtRunMode runMode;
    bool g_isDevice = false;
    aclError aclRet = aclrtGetRunMode(&runMode);
    g_isDevice = (runMode == ACL_DEVICE);

    // 3. 设置单算子模型文件所在的目录
    // 该目录相对可执行文件所在的目录，例如，编译出来的可执行文件存放在run/out目录下，此处就表示run/out/op_models目录
    aclopSetModelDir("op_models");

    // 4. 申请内存
    // 申请Device上的内存存放执行算子的输入数据
    // 对于该矩阵乘示例，依次申请存放矩阵A数据、矩阵B数据、矩阵C数据、标量α数据、标量β数据的内存
    aclrtMalloc((void **) &devMatrixA_, sizeA_, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc((void **) &devMatrixB_, sizeB_, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc((void **) &devMatrixC_, sizeC_, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc((void **) &devAlpha_, sizeAlphaBeta_, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc((void **) &devBeta_, sizeAlphaBeta_, ACL_MEM_MALLOC_HUGE_FIRST);

    // 申请Host上的内存，此处根据软件栈的运行模式判断是否需要申请Host上的内存
    // 如果运行模式为ACL_DEVICE，则g_isDevice参数值为true，表示软件栈运行在Device侧，无需申请Host内存，无需传输图片数据或在Device内传输数据
    // 如果运行模式为ACL_HOST，则g_isDevice参数值为false，表示软件栈运行在Host侧，需要申请Host内存，涉及Host和Device之间的数据传输
    if (g_isDevice) {
            hostMatrixA_ = devMatrixA_;
            hostMatrixB_ = devMatrixB_;
            hostMatrixC_ = devMatrixC_;
        } else {
            aclrtMallocHost((void **) &hostMatrixA_, sizeA_);
            aclrtMallocHost((void **) &hostMatrixB_, sizeB_);
            aclrtMallocHost((void **) &hostMatrixC_, sizeC_);
        }

    // 5. 准备输入数据，ReadFile为自定义函数，由用户自行管理，从文件中读入数据到内存中
    size_t fileSize;
    // Read matrix A
    char *fileData = ReadFile("test_data/data/matrix_a.bin", fileSize, hostMatrixA_, sizeA_);
    // Read matrix B
    fileData = ReadFile("test_data/data/matrix_b.bin", fileSize, hostMatrixB_, sizeB_);
    // Read matrix C
    fileData = ReadFile("test_data/data/matrix_c.bin", fileSize, hostMatrixC_, sizeC_);
    // 根据软件栈的运行模式判断是否涉及Host与Device之间的数据传输
    if (!g_isDevice) {
        aclError ret = aclrtMemcpy(devMatrixA_, sizeA_, hostMatrixA_, sizeA_, ACL_MEMCPY_HOST_TO_DEVICE);
        ret = aclrtMemcpy(devMatrixB_, sizeB_, hostMatrixB_, sizeB_, ACL_MEMCPY_HOST_TO_DEVICE);
        ret = aclrtMemcpy(devMatrixC_, sizeC_, hostMatrixC_, sizeC_, ACL_MEMCPY_HOST_TO_DEVICE);
    }

    aclrtMemcpyKind kind = g_isDevice ? ACL_MEMCPY_DEVICE_TO_DEVICE : ACL_MEMCPY_HOST_TO_DEVICE;
    ret = aclrtMemcpy(devAlpha_, sizeAlphaBeta_, hostAlpha_, sizeAlphaBeta_, kind);
    ret = aclrtMemcpy(devBeta_, sizeAlphaBeta_, hostBeta_, sizeAlphaBeta_, kind);

    // 6. 执行单算子
    // 对于该示例，调用aclblasGemmEx接口（异步接口）实现矩阵-矩阵的乘法
    aclblasGemmEx(ACL_TRANS_N, ACL_TRANS_N, ACL_TRANS_N, m_, n_, k_,
                        devAlpha_, devMatrixA_, k_, inputType_, devMatrixB_, n_, inputType_,
                        devBeta_, devMatrixC_, n_, outputType_, ACL_COMPUTE_HIGH_PRECISION,
                        stream);
    // 调用aclrtSynchronizeStream接口阻塞Host运行，直到指定Stream中的所有任务都完成
    aclrtSynchronizeStream(nullptr);

    // 7. 传输算子执行结果，根据软件栈的运行模式判断是否涉及Host与Device之间的数据传输
    if (!g_isDevice) {
            auto ret = aclrtMemcpy(hostMatrixC_, sizeC_, devMatrixC_, sizeC_, ACL_MEMCPY_DEVICE_TO_HOST);
    }

    // 8. 是否直接在终端屏幕上显示算子执行结果，由用户自行管理代码逻辑

    // 9. 释放运行时资源（默认Context、Stream无需用户释放，调用aclrtResetDevice接口后自动释放）
    aclRet = aclrtResetDevice(0);

    // 10.去初始化
    aclRet = aclFinalize();

    // ......
    ```
