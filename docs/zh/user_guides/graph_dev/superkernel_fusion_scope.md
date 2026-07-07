# SuperKernel融合范围标定

## 简介

### 功能介绍

SuperKernel是一种基于二进制层面的算子融合技术，区别于传统的源码级融合，其核心在于对已编译的二进制内核函数（Kernel）进行深度优化。通过智能调度与组合，将多个独立的内核函数（子Kernel）整合为一个高效的“超级内核”（SuperKernel），以调用子函数的形式统一调用多个其他内核函数。相比单个算子下发，SuperKernel技术显著降低了任务调度的等待时间与调度开销，并能有效利用子任务间的空闲资源，进一步优化整体算子的执行效率和头开销。

为提升SuperKernel融合的灵活性，引入了一种基于Scope标记的SuperKernel标定机制：用户可手动指定需要融合到同一SuperKernel中的算子，同时支持手动标记不参与融合的算子，且“不融合”标记具有最高优先级。

<!-- npu="A3" id1 -->
**该特性目前仅支持Atlas A3 训练系列产品/Atlas A3 推理系列产品。**
<!-- end id1 -->

### 实现原理

当检测到用户进行了手动标定时，系统将按照用户标定的范围进行SuperKernel融合。在SuperKernel生成策略上，系统将根据其标记集合进行分组。具有完全相同标记集合的算子将被归为一组，进入后续的Super Kernel融合流程。工作流程为：

![](figures/network_topo_example.png)

- 记录标定信息

    系统遍历所有算子，记录每个算子被用户标定的“标记集合”（即哪些算子属于同一标定区域）。

- 按标记集合分组

    将所有算子按照其“标记集合”完全一致的原则进行分组。只有标记集合完全相同的算子才会被归入同一组。

- 触发融合流程

    每组内算子进入SuperKernel的融合逻辑（如下图中的sk1、sk2等）。

如下两个图展示了SuperKernel融合范围的标定示例：

- 标定SuperKernel融合范围

    **图 1**  标记SuperKernel融合范围的示意图
    ![](figures/fusion_scope_diagram.png "标记SuperKernel融合范围的示意图")

  - 使用“sk1”将op3、op4、op5标定到一个融合范围。
  - 使用“sk2”将op7、op8、op9标定到一个融合范围。
  - op1、op2、op6由于未被纳入任何标定组，不参与融合。

- 标定SuperKernel不融合的算子

    **图 2**  标记算子不融合的示意图
    ![](figures/mark_op_no_fusion_diagram.png "标记算子不融合的示意图")

    使用None标定的算子：op3、op4、op5不参与SuperKernel的融合。

### 使用约束

标定范围内若存在不可融合的算子，会生成第一段SuperKernel，同时自动跳过该算子进行第二段的SuperKernel融合，因此标定范围不代表最终的融合结果。

## 融合范围标定示例

通过调用[aclskScopeBegin](../../api/graph_engine_api/c/super_kernel/aclskScopeBegin.md)和[aclskScopeEnd](../../api/graph_engine_api/c/super_kernel/aclskScopeEnd.md)接口，用户若传入有效scopeName可以框定算子融合进SuperKernel，若传入nullptr可以框定算子不融合。该功能只在开启SuperKernel优化时有实际效果。如下代码演示了如何在NPU上利用SuperKernel技术实现一个简单的加法算子。

- **标定算子融合到SuperKernel的关键步骤示例**

    代码仅展示核心逻辑，不可直接编译运行，仅供参考：

    ```c++
    #include "acl/acl.h"
    #include "super_kernel/super_kernel.h"
    ...
    // 原始kernel函数（add_custom）
    __global__ __vector__ void add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z, AddCustomTilingData tiling)
    {
        KernelAdd op;
        op.Init(x, y, z, tiling.totalLength, tiling.tileNum);
        op.Process();
    }
    // 定义参数结构体，用于封装传递给SuperKernel的参数
    struct ArgsStruct {
        GM_ADDR x;
        GM_ADDR y;
        GM_ADDR z;
        AddCustomTilingData tiling;
    };
    // 定义一个带模板参数的SuperKernel子函数（add_custom_sk）
    template<uint32_t splitNum>
    __sk__ __vector__ void add_custom_sk(const ArgsStruct *args, sk::SkSystemArgs *sysArgs)
    {
        // 从结构体获取参数
        GM_ADDR x = args->x;
        GM_ADDR y = args->y;
        GM_ADDR z = args->z;
        AddCustomTilingData tiling = args->tiling;
        uint16_t blockNum = sysArgs->skNumBlocks;
        uint16_t blockIdx = sysArgs->skBlockIdx;
        // 逻辑与原kernel的global函数一致
        KernelAdd op;
        op.Init(x, y, z, tiling.totalLength, tiling.tileNum);
        op.Process();
    }

    // 使用SK_BIND绑定，将add_custom kernel与多个SK子函数绑定，通过指定模板参数实例化出4个不同的符号
    SK_BIND(add_custom, 4, add_custom_sk<0>, add_custom_sk<1>, add_custom_sk<2>, add_custom_sk<3>);

    // 主函数
    int main()
    {
        // 算子数据准备
        constexpr uint32_t totalLength = 8 * 2048;
        constexpr float valueX = 1.2f;
        constexpr float valueY = 2.3f;
        std::vector<float> x(totalLength, valueX);
        std::vector<float> y(totalLength, valueY);
        constexpr uint32_t numBlocks = 8;
        uint32_t totalLength = x.size();
        size_t totalByteSize = totalLength * sizeof(float);
        int32_t deviceId = 0;
        aclrtStream stream = nullptr;
        AddCustomTilingData tiling = {totalLength, 8};
        uint8_t *xHost = reinterpret_cast<uint8_t *>(x.data());
        // printf("0-host: x0:%d \n", xHost[0]);
        uint8_t *yHost = reinterpret_cast<uint8_t *>(y.data());
        uint8_t *zHost = nullptr;
        uint8_t *xDevice = nullptr;
        uint8_t *yDevice = nullptr;
        uint8_t *zDevice = nullptr;
        // 内存分配
        aclrtMallocHost((void **)(&zHost), totalByteSize);
        aclrtMalloc((void **)&xDevice, totalByteSize, ACL_MEM_MALLOC_HUGE_FIRST);
        aclrtMalloc((void **)&yDevice, totalByteSize, ACL_MEM_MALLOC_HUGE_FIRST);
        aclrtMalloc((void **)&zDevice, totalByteSize, ACL_MEM_MALLOC_HUGE_FIRST);
        // 数据传输
        aclrtMemcpy(xDevice, totalByteSize, xHost, totalByteSize, ACL_MEMCPY_HOST_TO_DEVICE);
        aclrtMemcpy(yDevice, totalByteSize, yHost, totalByteSize, ACL_MEMCPY_HOST_TO_DEVICE);
        // 初始化
        aclInit(nullptr);
        aclrtSetDevice(deviceId);
        aclrtCreateStream(&stream);
        // 构建图
        aclmdlRICaptureBegin(stream, ACL_MODEL_RI_CAPTURE_MODE_GLOBAL);
        aclmdlRI modelRI;
        // 标定SuperKernel融合起始位置
        aclskScopeBegin("sk1", stream);
        add_custom<<<numBlocks, nullptr, stream>>>(xDevice, yDevice, zDevice, tiling);
        // 标定SuperKernel融合结束位置
        aclskScopeEnd("sk1", stream);
        aclmdlRICaptureEnd(stream, &modelRI);
        // 开启SuperKernel优化
        aclskOptimize(modelRI, nullptr);
        // 执行图
        aclmdlRIExecuteAsync(modelRI, stream);
        // 获取图执行结果
        aclrtSynchronizeStream(stream);
        aclrtMemcpy(zHost, totalByteSize, zDevice, totalByteSize, ACL_MEMCPY_DEVICE_TO_HOST);
        std::vector<float> z((float *)zHost, (float *)(zHost + totalLength));
        // 资源释放
        aclrtFree(xDevice);
        aclrtFree(yDevice);
        aclrtFree(zDevice);
        aclrtFreeHost(zHost);
        aclrtDestroyStream(stream);
        aclrtResetDevice(deviceId);
        aclFinalize();
    }
    ```

    上述acl接口详细说明请参见[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityRuntimeApi)。

- **标定算子不融合到SuperKernel的关键步骤示例**

    以下代码仅展示核心逻辑，不可直接编译运行，仅供参考：

    ```c++
    #include "acl/acl.h"
    #include "super_kernel/super_kernel.h"
    ...
    int main()
    {
        // 算子数据准备
        ...
        // 初始化
        aclInit(nullptr);
        aclrtSetDevice(deviceId);
        aclrtCreateStream(&stream);
        // 构建图
        aclmdlRICaptureBegin(stream, ACL_MODEL_RI_CAPTURE_MODE_GLOBAL);
        aclmdlRI modelRI;
        // 标定SuperKernel融合起始位置，传入nullptr表示不融合
        aclskScopeBegin(nullptr, stream);
        add_custom<<<numBlocks, nullptr, stream>>>(xDevice, yDevice, zDevice, tiling);
        // 标定SuperKernel融合结束位置，传入nullptr表示不融合
        aclskScopeEnd(nullptr, stream);
        aclmdlRICaptureEnd(stream, &modelRI);
        // 开启SuperKernel优化
        aclskOptimize(modelRI, nullptr);
        // 执行图
        aclmdlRIExecuteAsync(modelRI, stream);
        // 获取图执行结果
        ...
        // 资源释放
        aclrtFree(xDevice);
        aclrtFree(yDevice);
        aclrtFree(zDevice);
        aclrtFreeHost(zHost);
        aclrtDestroyStream(stream);
        aclrtResetDevice(deviceId);
        aclFinalize();
    }
    ```

    上述acl接口详细说明请参见[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityRuntimeApi)。
