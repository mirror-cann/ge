# Session到GeSession的迁移指导

本节旨在指导用户从Session类迁移到新引入的GeSession类。GeSession是对原有Session类的重构和优化，主要变化包括：

- 简化编译和加载流程：不需要手动调用CompileGraph和LoadGraph（除非需要显式控制）。详见[编译和运行接口变化](#编译和运行接口变化)。
- 统一执行接口的参数类型：所有运行接口的输入输出从ge::Tensor改为gert::Tensor。详见[Tensor类型变化](#tensor类型变化)。
- 优化接口命名和参数类型。详见[接口变化](#接口变化)。

其他变更点注意事项：

- 头文件和库文件变化：头文件从ge/ge\_api.h改为ge/ge\_api\_v2.h，库文件从libge\_runner.so改为libge\_runner\_v2.so。详见[库链接变化](#库链接变化)。
- 异步生命周期：使用RunGraphAsync时，inputs必须保持有效直到callback被调用。详见[Tensor生命周期说明](#li1)。
- 执行模式互斥：三种运行模式不能混用。详见[编译和运行接口变化](#编译和运行接口变化)。

下面按照分类详细介绍上述变更点。

## 库链接变化

**表 1**  库链接变化

|类别|Session|GeSession|
|--|--|--|
|库文件|libge_runner.so|libge_runner_v2.so|
|头文件|ge/ge_api.h|ge/ge_api_v2.h|

## 接口变化

| Session接口 | GeSession接口 | 迁移说明 |
| --- | --- | --- |
| [GEInitialize](../../../api/graph_engine_api/cpp/ge/Session/GEInitialize.md)(options) | [GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)(options) | 新增接口 |
| [GEFinalize](../../../api/graph_engine_api/cpp/ge/Session/GEFinalize.md)() | [GEFinalizeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEFinalizeV2.md)() | 新增接口 |
| [GEGetErrorMsg](../../../api/graph_engine_api/cpp/ge/Session/GEGetErrorMsg.md)() | [GEGetErrorMsgV3](../../../api/graph_engine_api/cpp/ge/GeSession/GEGetErrorMsgV3.md)() | 新增接口 |
| [GEGetWarningMsg](../../../api/graph_engine_api/cpp/ge/Session/GEGetWarningMsg.md)() | [GEGetWarningMsgV3](../../../api/graph_engine_api/cpp/ge/GeSession/GEGetWarningMsgV3.md)() | 新增接口 |
| [Session](../../../api/graph_engine_api/cpp/ge/Session/Session.md)(options) | [GeSession](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)(options) | 构造函数基本一致，但只提供了ABI兼容的std::map<AscendString, AscendString>类型版本 |
| ~Session() | ~GeSession() | 析构函数，无变化 |
| [AddGraph](../../../api/graph_engine_api/cpp/ge/Session/AddGraph.md)(uint32_t, const Graph&) | [AddGraph](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraph.md)<br>(uint32_t, const Graph&) | 接口保持一致 |
| [AddGraph](../../../api/graph_engine_api/cpp/ge/Session/AddGraph.md)(uint32_t, const Graph&, options) | [AddGraph](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraph.md)<br>(uint32_t, const Graph&, options) | 只提供了ABI兼容的std::map<AscendString, AscendString>类型版本 |
| [AddGraphWithCopy](../../../api/graph_engine_api/cpp/ge/Session/AddGraphWithCopy.md) | [AddGraphClone](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraphClone.md) | 重命名，功能相同 |
| [RemoveGraph](../../../api/graph_engine_api/cpp/ge/Session/RemoveGraph.md) | [RemoveGraph](../../../api/graph_engine_api/cpp/ge/GeSession/RemoveGraph.md) | 无变化 |
| [BuildGraph](../../../api/graph_engine_api/cpp/ge/Session/BuildGraph.md) | [CompileGraph](../../../api/graph_engine_api/cpp/ge/GeSession/CompileGraph.md) | 重命名，功能相同。GeSession的CompileGraph支持Variable |
| [CompileGraph](../../../api/graph_engine_api/cpp/ge/Session/CompileGraph.md) | [CompileGraph](../../../api/graph_engine_api/cpp/ge/GeSession/CompileGraph.md) | 合并了BuildGraph和CompileGraph的功能 |
| [LoadGraph](../../../api/graph_engine_api/cpp/ge/Session/LoadGraph.md) | [LoadGraph](../../../api/graph_engine_api/cpp/ge/GeSession/LoadGraph.md) | 接口基本一致，但GeSession中会自动检查是否需要先CompileGraph |
| [RunGraph](../../../api/graph_engine_api/cpp/ge/Session/RunGraph.md) | [RunGraph](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraph.md) | 重要变化：输入输出从ge::Tensor改为gert::Tensor |
| [RunGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/Session/RunGraphWithStreamAsync.md) | [RunGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraphWithStreamAsync.md) | 重要变化：输入输出从ge::Tensor改为gert::Tensor；CompileGraph和LoadGraph可省略 |
| [ExecuteGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/Session/ExecuteGraphWithStreamAsync.md) | [RunGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraphWithStreamAsync.md) | Session中的ExecuteGraphWithStreamAsync（使用gert::Tensor）合并到GeSession的RunGraphWithStreamAsync |
| [RunGraphAsync](../../../api/graph_engine_api/cpp/ge/Session/RunGraphAsync.md) | [RunGraphAsync](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraphAsync.md) | 重要变化：输入输出从ge::Tensor改为gert::Tensor；回调函数签名从RunAsyncCallback改为RunAsyncCallbackV2 |
| [RegisterCallBackFunc](../../../api/graph_engine_api/cpp/ge/Session/RegisterCallBackFunc.md) | [RegisterCallBackFunc](../../../api/graph_engine_api/cpp/ge/GeSession/RegisterCallBackFunc.md) | 回调函数签名变化，使用RunCallback类型 |
| [GetCompiledGraphSummary](../../../api/graph_engine_api/cpp/ge/Session/GetCompiledGraphSummary.md) | [GetCompiledGraphSummary](../../../api/graph_engine_api/cpp/ge/GeSession/GetCompiledGraphSummary.md) | 无变化 |
| [SetGraphConstMemoryBase](../../../api/graph_engine_api/cpp/ge/Session/SetGraphConstMemoryBase.md) | [SetGraphConstMemoryBase](../../../api/graph_engine_api/cpp/ge/GeSession/SetGraphConstMemoryBase.md) | 无变化 |
| [UpdateGraphFeatureMemoryBase](../../../api/graph_engine_api/cpp/ge/Session/UpdateGraphFeatureMemoryBase.md) | [UpdateGraphFeatureMemoryBase](../../../api/graph_engine_api/cpp/ge/GeSession/UpdateGraphFeatureMemoryBase.md) | 无变化 |
| [SetGraphFixedFeatureMemoryBase](../../../api/graph_engine_api/cpp/ge/Session/SetGraphFixedFeatureMemoryBase.md) | [SetGraphFixedFeatureMemoryBaseWithType](../../../api/graph_engine_api/cpp/ge/GeSession/SetGraphFixedFeatureMemoryBaseWithType.md) | 接口名称变化，增加了type参数 |
| [UpdateGraphRefreshableFeatureMemoryBase](../../../api/graph_engine_api/cpp/ge/Session/UpdateGraphRefreshableFeatureMemoryBase.md) | [UpdateGraphRefreshableFeatureMemoryBase](../../../api/graph_engine_api/cpp/ge/GeSession/UpdateGraphRefreshableFeatureMemoryBase.md) | 无变化 |
| [RegisterExternalAllocator](../../../api/graph_engine_api/cpp/ge/Session/RegisterExternalAllocator.md) | [RegisterExternalAllocator](../../../api/graph_engine_api/cpp/ge/GeSession/RegisterExternalAllocator.md) | 无变化 |
| [UnregisterExternalAllocator](../../../api/graph_engine_api/cpp/ge/Session/UnregisterExternalAllocator.md) | [UnregisterExternalAllocator](../../../api/graph_engine_api/cpp/ge/GeSession/UnregisterExternalAllocator.md) | 无变化 |
| [IsGraphNeedRebuild](../../../api/graph_engine_api/cpp/ge/Session/IsGraphNeedRebuild.md) | [IsGraphNeedRebuild](../../../api/graph_engine_api/cpp/ge/GeSession/IsGraphNeedRebuild.md) | 无变化 |
| [GetSessionId](../../../api/graph_engine_api/cpp/ge/Session/GetSessionId.md) | [GetSessionId](../../../api/graph_engine_api/cpp/ge/GeSession/GetSessionId.md) | 无变化 |
| - | [GetCompiledModel](../../../api/graph_engine_api/cpp/ge/GeSession/GetCompiledModel.md) | 新增接口：获取编译后的模型数据 |
| [GetVariables](../../../api/graph_engine_api/cpp/ge/Session/GetVariables.md) | - | 已删除，无替代接口 |
| [ShardGraphsToFile](../../../api/graph_engine_api/cpp/ge/Session/ShardGraphsToFile.md) | - | 已删除，图分片功能不再提供 |
| [ShardGraphs](../../../api/graph_engine_api/cpp/ge/Session/ShardGraphs.md) | - | 已删除，图分片功能不再提供 |
| [SaveGraphsToPb](../../../api/graph_engine_api/cpp/ge/Session/SaveGraphsToPb.md) | - | 已删除，保存图到pb文件功能不再提供 |
| [PaRemapped](../../../api/graph_engine_api/cpp/ge/Session/PaRemapped.md) | - | 已删除，虚拟内存重映射功能不再提供 |

## Tensor类型变化

所有运行接口的输入输出从ge::Tensor改为gert::Tensor，详情如下：

- 命名空间变化

    |特性|ge::Tensor|gert::Tensor|
    |--|--|--|
    |命名空间|ge|gert|
    |数据结构|使用std::shared_ptr\<TensorImpl\>管理内部实现|POD类型（Plain Old Data），所有数据内联存储|
    |内存布局|间接访问，通过impl_指针|扁平化布局，支持直接memcpy|
    |Placement支持|通过TensorDesc设置Placement|支持多种Placement类型|
    |拷贝行为|浅拷贝（shared_ptr语义）|浅拷贝，指针共享|
    |性能|一般|高性能|
    |适用场景|图构建阶段|运行时执行|

- 数据结构变化

  - gert::Tensor内部结构

    ```c++
    class Tensor {
    private:
      std::shared_ptr<TensorImpl> impl_;  // 使用智能指针管理
    };
    ```

    - 使用shared_ptr管理TensorImpl
    - 拷贝时共享底层实现
    - 数据通过TensorDesc描述

  - ge::Tensor内部结构

    ```c++
    class Tensor {
    private:
     StorageShape storage_shape_;        // Shape信息
      StorageFormat storage_format_;      // Format信息
      TensorVersion version_;             // 版本
     uint8_t reserved_[3];               // 预留字段
     ge::DataType data_type_;            // 数据类型
     TensorData tensor_data_;            // 数据指针和placement
     uint8_t reserved_field_[40];        // 预留字段
    };
    ```

    - 所有字段直接内联在对象中
    - 是标准布局类型（std::is_standard_layout）

下面给出gert::Tensor生命周期的说明以及构造gert::Tensor的方法：

- Tensor生命周期说明<a id="li1"></a>
  - RunGraph接口

    ```c++
    std::vector<gert::Tensor> inputs = ...;
    std::vector<gert::Tensor> outputs;
    session->RunGraph(graph_id, inputs, outputs);
    // inputs和outputs在调用完成后可以安全释放
    ```

  - RunGraphWithStreamAsync接口

    ```c++
    // 注意：可以不先调用CompileGraph和LoadGraph，会自动处理
    std::vector<gert::Tensor> inputs = ...;
    std::vector<gert::Tensor> outputs;
    session->RunGraphWithStreamAsync(graph_id, stream, inputs, outputs);
    // inputs和outputs在stream同步之前不能释放
    // 需要调用aclrtSynchronizeStream或其他同步机制
    ```

  - RunGraphAsync接口（重要）

    ```c++
    using RunAsyncCallbackV2 = std::function<void(Status, std::vector<gert::Tensor>&)>;
    std::vector<gert::Tensor> inputs = ...;
    session->RunGraphAsync(graph_id, inputs,
        [](Status ret, std::vector<gert::Tensor>& outputs) {
            // 处理输出
        });
    // 重要：inputs不能立即释放！
    // 必须等到callback函数被调用后才能释放inputs
    // 因为模型执行的时候会读取inputs、callback被调用可保证模型执行完成
    ```

- 构造gert::Tensor的方法
  - 方法1: 使用TensorData构造（推荐）

    ```c++
    #include "exe_graph/runtime/tensor.h"
    #include "acl_rt.h"

    // 构造 Host Tensor
    void* host_buf = nullptr;
    aclError ret = aclrtMallocHost(&host_buf, data_len);  // 分配Host内存
    if (ret != ACL_ERROR_NONE) {
        // 处理错误
    }

    // 使用TensorData构造
    gert::TensorData td(host_buf, nullptr, data_len, gert::kOnHost);
    gert::Tensor tensor;
    tensor.SetData(std::move(td));

    // 设置数据类型（如果需要）
    // tensor.SetDataType(ge::DT_FLOAT);
    ```

  - 方法2: 使用构造函数直接创建

    ```c++
    // 从 shape、format 和 dtype 构造
    gert::StorageShape shape = {{batch_size, channels, height, width}, {4}};
    gert::StorageFormat format = {ge::FORMAT_ND, ge::FORMAT_ND, {}};
    gert::Tensor tensor(shape, format, ge::DT_FLOAT);

    // 然后分配内存
    void* host_buf = nullptr;
    aclrtMallocHost(&host_buf, tensor.GetSize());
    gert::TensorData td(host_buf, nullptr, tensor.GetSize(), gert::kOnHost);
    tensor.SetData(std::move(td));
    ```

  - 方法3: 构造Device Tensor

    ```c++
    // 分配Device内存
    void* dev = nullptr;
    aclError ret = aclrtMalloc(&dev, bytes, ACL_MEM_MALLOC_NORMAL_ONLY);
    if (ret != ACL_ERROR_NONE) {
        // 处理错误
    }

    // 构造 Device Tensor
    gert::TensorData td(dev, nullptr, bytes, gert::kOnDeviceHbm);
    gert::Tensor device_tensor;
    device_tensor.SetData(std::move(td));
    ```

- 释放gert::Tensor内存

    ```c++
    // 释放Host Tensor内存
    void FreeHostTensor(gert::Tensor &tensor) {
        if (tensor.GetAddr() != nullptr) {
            aclrtFreeHost(tensor.GetAddr());
        }
    }

    // 释放Device Tensor内存
    void FreeDeviceTensor(gert::Tensor &tensor) {
        if (tensor.GetAddr() != nullptr) {
            aclrtFree(tensor.GetAddr());
        }
    }

    // 批量释放
    void FreeTensorVector(std::vector<gert::Tensor> &tensors, bool is_device) {
        for (auto &t : tensors) {
            if (t.GetAddr() != nullptr) {
                if (is_device) {
                    aclrtFree(t.GetAddr());
                } else {
                    aclrtFreeHost(t.GetAddr());
                }
            }
        }
    }
    ```

aclrtMallocHost、aclrtMalloc、aclrtFreeHost、aclrtFree等接口详细说明请参见[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclmalloc)中的“内存管理”。

## 编译和运行接口变化

- CompileGraph/LoadGraph不再必需

    在GeSession中，RunGraph、RunGraphAsync和RunGraphWithStreamAsync三个执行接口会自动检查图是否已编译和加载。如果未编译，会先自动编译；如果未加载，会先自动加载。

    ```c++
    // GeSession的自动处理机制
    GeSession session(options);
    session.AddGraph(graph_id, graph);

    // 直接执行，无需手动CompileGraph和LoadGraph
    session.RunGraph(graph_id, inputs, outputs);  // 自动编译和加载
    ```

- 执行模式互斥

    GeSession的三种执行模式RunGraph、RunGraphAsync、RunGraphWithStreamAsync是互斥的，不能混用。一旦使用了某种执行模式，该图就必须继续使用同一种模式。

## 编译配置变化

Makefile或CMakeLists.txt需要更新：

|Session接口|GeSession接口|
|--|--|
|# 旧配置<br>target_link_libraries(your_app libge_runner.so)|# 新配置<br>target_link_libraries(your_app libge_runner_v2.so)|
