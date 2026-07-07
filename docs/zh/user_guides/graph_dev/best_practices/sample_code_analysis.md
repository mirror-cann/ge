# 样例代码解析

本实践采用模块化设计，基于C++语言结合GE图引擎 API与acl（Ascend Computing Language）API实现，完整代码请单击[推荐网络高性能示例](https://gitcode.com/cann/ge/tree/master/examples/recommendation)获取，核心组件构成如下：

1. **ModelInference::Builder**：构建器，配置模型参数；封装ModelInference对象的构建过程，提供链式配置接口。
2. **ModelInference**：核心类，提供模型初始化、资源管理、任务调度等核心能力。
3. **ModelInference::GraphWorker**：工作线程，执行异步推理任务的线程单元。
4. **ModelInference::GraphTask**：任务单元，封装单次推理请求（输入/输出和回调）的完整生命周期。

下图为各个组件的UML（Unified Modeling Language，统一建模语言）类图：

**图 1**  样例代码结构
![图1示例](../figures/sample_code_structure.png "样例代码结构")

## 接口调用流程

样例代码主要执行流程以及涉及接口如下：

![图2示例](../figures/single_thread_14.png)

1. 调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclinit)中的“初始化和去初始化 \> aclInit”接口，初始化acl，调用《Runtime运行时 API》中的“Device管理 \> aclrtSetDevice”指定运行的Device。
2. 构建ModelInference实例并初始化特性开关：开启批量H2D功能、配置AICore控核策略、使用多实例并行。
3. ModelInference初始化：
    1. 调用[Session构造函数](../../../api/graph_engine_api/cpp/ge/Session/Session.md)创建Session类对象，申请Session资源，Session中的options中配置ge.aicoreNum参数。
    2. 调用[GEInitialize](../../../api/graph_engine_api/cpp/ge/Session/GEInitialize.md)进行系统初始化。
    3. 调用[aclgrphParseTensorFlow](../../../api/graph_engine_api/cpp/ge/aclgrphParseTensorFlow.md)解析模型，获取Graph。
    4. 调用[AddGraph](../../../api/graph_engine_api/cpp/ge/Session/AddGraph.md)在Session类对象中添加定义好的图。
    5. 调用[CompileGraph](../../../api/graph_engine_api/cpp/ge/Session/CompileGraph.md)完成图编译。
    6. 调用《Runtime运行时 API》中的“Device管理 \> aclrtGetDevice”获取运行的Device。
    7. 创建多个线程，每个线程传入相同的Session，Graph ID，Device ID。

4. 提交推理任务到工作线程。下面以一个线程为例，描述工作线程的执行流程：
    1. 调用《Runtime运行时 API》中的“Device管理 \> aclrtSetDevice”指定运行的Device，调用“aclrtCreateStream”创建Stream。
    2. 调用[LoadGraph](../../../api/graph_engine_api/cpp/ge/Session/LoadGraph.md)（异步执行Graph场景），将图模型加载到上一步骤创建的Stream上。监听任务队列接收并执行任务：
        1. 调用《Runtime运行时 API》中的“内存管理 \> aclrtMalloc”申请Device内存，调用《Runtime运行时 API》中的“内存管理 \> aclrtMemcpyBatch”将数据从Host批量传输到Device。（如果开启批量H2D功能，使用aclrtMemcpyBatch接口，不开启该功能，则使用aclrtMemcpy接口）。
        2. 调用[ExecuteGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/Session/ExecuteGraphWithStreamAsync.md)异步执行接口，运行Graph。
        3. 调用“aclrtSynchronizeStream”阻塞程序运行，直到指定Stream中的所有任务都完成。
        4. 调用aclrtMemcpyBatch将数据从Device批量回传到Host。
        5. 调用《Runtime运行时 API》中的“内存管理 \> aclrtFree”释放内存。
        6. 执行自定义的回调函数。

5. 调用[GEFinalize](../../../api/graph_engine_api/cpp/ge/Session/GEFinalize.md)，释放系统资源；调用《Runtime运行时 API》中的“初始化和去初始化 \> aclFinalize”释放相关资源。

## 开发示例

1. 包含的头文件，包括acl、C或C++标准库、GE、样例ModelInference的头文件。

    ```c++
    #include <acl.h>
    #include <acl_rt.h>
    #include <sstream>
    #include <random>
    #include <unordered_map>
    #include <chrono>
    #include <atomic>
    #include <complex>
    #include <iostream>
    #include <vector>
    #include <map>
    #include "model_inference.h"
    #include <getopt.h>
    #include <string>
    ```

2. acl资源初始化，设置Device。

    ```c++
    // 初始化acl
    aclError aerr = aclInit(nullptr);
    if (aerr != ACL_ERROR_NONE) {
      std::cerr << "Failed to init ACL, error=" << aerr << std::endl;
      return -1;
    }
    // 指定用于运算的Device
    aerr = aclrtSetDevice(0);
    if (aerr != ACL_ERROR_NONE) {
      std::cerr << "aclrtSetDevice failed, ret=" << aerr << std::endl;
      aclFinalize();
      return -1;
    }
    ```

3. 设置推理参数。

    ```c++
    // 模型文件路径
    const std::string model_path = "../data/DCN_v2.pb";
    // 模型文件类型
    const std::string model_type = "TensorFlow";
    ```

4. 指定构造模型解析参数，样例模型输入数量为27个。

    ```c++
    std::stringstream ss;
    // 定义输入节点的数量
    int input_size = 27;
    for (int i = 1; i < input_size; ++i) ss << "Input_" << i << ":" << batchSize << ";";
    ss << "Input:" << batchSize << ",8";
    // 构建一个map，用于配置模型的解析参数
    std::map<ge::AscendString, ge::AscendString> parser = {
        //  设置输出节点
        {ge::AscendString(ge::ir_option::OUT_NODES),
         ge::AscendString("Identity:0")},
        // 设置输入shape
        {ge::AscendString(ge::ir_option::INPUT_SHAPE),
         ge::AscendString(ss.str().c_str())}
    };
    ```

5. 构建ModelInference实例并初始化。

    ```c++
    // 创建 ModelInference实例
    auto model_inference = gerec::ModelInference::Builder(cfg.model_path, cfg.model_type)
                           .InputBatchCopy(enableBatchH2D)        // 开启批量H2D功能
                           .AiCoreNum(aiCoreNum)                  // 配置AICore控核
                           .MultiInstanceNum(multiInstanceNum)    // 多实例并行
                           .GraphParserParams(cfg.parser_params)  // 设置图解析参数
                           .Build();
    if (model_inference->Init() != ge::SUCCESS) {
      std::cerr << "Init ModelInference failed" << std::endl;
      return ge::FAILED;
    }
    ```

6. 提交推理任务。

    ```c++
    // 回调函数，用于在异步推理完成后执行清理和统计工作
    auto callback = [&](std::shared_ptr<std::vector<gert::Tensor>> outputs,
                        std::shared_ptr<std::vector<gert::Tensor>> inputs, bool status, long long exec_us) {
      if (status) {
        // 如果推理成功，更新成功计数和执行时间总和
        success_count.fetch_add(1, std::memory_order_relaxed);        // 成功次数增加
        total_exec_us.fetch_add(exec_us, std::memory_order_relaxed);  // 累加执行时间（微秒）
      }
      // 释放输出/输入Tensor占用的内存
      FreeHostTensors(outputs);
      FreeHostTensors(inputs);
    };

    // 执行多次异步推理
    for (int i = 0; i < num_runs; ++i) {
      if (model_inference->RunGraphAsync(all_inputs[i], all_outputs[i], callback) != ge::SUCCESS) {
        std::cerr << "RunGraphAsync failed at " << i << std::endl;
        return ge::FAILED;
      }
    }
    ```

    > [!NOTE]说明
    >RunGraphAsync接口采用异步执行模式，需绑定回调函数（Callback）以处理推理结果。回调函数需满足以下签名规范：
    >
    >```c++
    >using Callback = std::function<void(
    >    std::shared_ptr<std::vector<gert::Tensor>> outputs,   // 输出Tensor列表
    >    std::shared_ptr<std::vector<gert::Tensor>> inputs,    // 输入Tensor列表
    >    bool status,                                          // 操作执行状态
    >    long long exec_us                                     // 执行时延（微秒）
    >    )>;
    >```

7. 释放资源。

    ```c++
    // acl去初始化
    aclFinalize();
    ```
