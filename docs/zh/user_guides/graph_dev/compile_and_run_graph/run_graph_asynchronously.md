# 异步运行Graph

## 单进程单卡异步运行

本章节给出使用异步运行接口，并使用Device内存运行Graph的示例。

### 功能介绍

构建完Graph之后，如果您希望**编译并异步运行Graph，获得Graph的运行结果**，请参考本节内容。涉及的主要接口为：

![图示](../figures/feature_intro_4.png)

1. 调用[GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)进行系统初始化（也可在Graph构建前调用），申请系统资源。
2. 调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclinit)中的“初始化和去初始化 \> aclInit”接口，初始化acl。
3. 调用[Session构造函数](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)创建Session类对象，申请Session资源。
4. 调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiacldevice)中的“Device管理 \> aclrtSetDevice”指定运行的Device，调用“aclrtCreateStream”创建Stream，然后调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclmalloc)中的“内存管理 \> aclrtMallocHost/aclrtMalloc”分别申请Host和Device内存。
5. 调用[AddGraph](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraph.md)在Session类对象中添加定义好的图。
6. （可选）调用[CompileGraph](../../../api/graph_engine_api/cpp/ge/GeSession/CompileGraph.md)完成图编译。
7. （可选）调用[LoadGraph](../../../api/graph_engine_api/cpp/ge/GeSession/LoadGraph.md)（异步执行Graph场景），加载图模型到上面步骤创建的Stream上。
8. 调用[RunGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraphWithStreamAsync.md)异步运行接口，运行Graph：

    若在调用本接口前未执行LoadGraph完成图加载，则本接口将自动调用LoadGraph以完成加载；若在调用LoadGraph接口前未执行CompileGraph完成图编译，则LoadGraph将自动调用CompileGraph以完成编译。

9. 调用“aclrtSynchronizeStream”阻塞程序运行，直到指定Stream中的所有任务都完成。
10. 调用《Runtime运行时 API》中的“内存管理 \> aclrtFree/aclrtFreeHost”释放内存；调用[GEFinalizeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEFinalizeV2.md)，释放系统资源；调用《Runtime运行时 API》中的“初始化和去初始化 \> aclFinalize”释放相关资源。

### 开发示例

1. 包含的头文件，包括acl、C或C++标准库的头文件。

    ```c++
    #include "ge_api_v2.h"
    #include "acl.h"
    #include "acl_rt.h"
    ```

2. 申请系统资源。

    Graph定义完成后，调用GEInitializeV2进行系统初始化（也可在Graph定义前调用），申请系统资源。示例代码如下：

    ```c++
    std::map<AscendString, AscendString>config = {{"ge.exec.deviceId", "0"},
                                                  {"ge.graphRunMode", "1"}};
    Status ret = ge::GEInitializeV2(config);
    ```

    可以通过config配置传入GE运行的初始化信息，配置参数ge.exec.deviceId和ge.graphRunMode，分别用于指定GE实例运行设备，图执行模式（在线推理请配置为0，训练请配置为1）。更多配置请参考[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/options_parameters_description.md)。

    > [!NOTE]说明
    >
    >GE options中的dump信息，与后续调用acl初始化接口时配置的dump信息，建议两者不要同时配置，否则可能导致异常。其他相同功能的参数类似。

3. acl资源初始化。

    ```c++
    std::string aclConfigPath = "xx/xx/xx";
    aclError retInit = aclInit(aclConfigPath);
    if (retInit != ACL_ERROR_NONE) {
        // ...
        // ...
        return FAILED;
    }
    ```

4. 创建Session。

    若想使定义好的Graph运行起来，首先，要创建一个Session对象。Session中的options可以加载配置参数，支持的配置参数请参见[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/options_parameters_description.md)。

    ```c++
    std::map <AscendString, AscendString> options;
    // 创建Session对象
    ge::GeSession* session = new GeSession(options);
    // 判断Session是否创建成功
    if(session == nullptr) {
      std::cout << "Create session failed." << std::endl;
      // ...
      // ...
      // 释放资源
      ge::GEFinalizeV2();
      return FAILED;
    }
    ```

5. 指定运行的Device，创建Stream，申请内存。

    ```c++
    // 指定用于运算的Device
    int32_t deviceId = 0;
    retInit = aclrtSetDevice(deviceId);

    // 创建一个Stream
    aclrtStream stream = nullptr;
    aclError aclRet = aclrtCreateStream(&stream);

    // 申请Host上的内存
    void* hostPtrA = NULL;
    size_t size = 1024;
    aclRet = aclrtMallocHost(&hostPtrA, size);
    // 申请Device上的内存
    void* devPtrB = NULL;
    aclRet = aclrtMalloc(&devPtrB, size, ACL_MEM_MALLOC_HUGE_FIRST);

    // 内存复制，将Host上数据传输到Device
    // hostPtrA表示Host上源内存地址指针，devPtrB表示Device上目的内存地址指针，size表示内存大小
    aclrtMemcpy(devPtrB, size, hostPtrA, size, ACL_MEMCPY_HOST_TO_DEVICE);

    ```

6. 添加Graph对象。

    调用AddGraph接口添加Graph。示例代码如下：

    ```c++
    // 准备将要添加到Session的Graph ID，并创建空的Graph对象
    uint32_t conv_graph_id = 0;
    ge::Graph conv_graph;
    // 将Graph添加到Session
    Status ret = session->AddGraph(conv_graph_id, conv_graph);
    if(ret != SUCCESS) {
      // ...
      // ...
      // 释放资源并销毁Session
      ge::GEFinalizeV2();
      delete session;
      return FAILED;
    }
    ```

    用户可以通过传入options配置图运行相关配置信息，相关配置请参考[Session构造函数](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)。其中图运行完之后的数据保存在Tensor output\_cov中。

7. （可选）编译Graph。

    如果不调用CompileGraph接口，LoadGraph接口将自动调用CompileGraph以完成编译。

    ```c++
    uint32_t graph_id = 0;
    ret = session->CompileGraph(graph_id);
    if(ret != SUCCESS) {
      // ...
      // ...
      // 释放资源
      ge::GEFinalizeV2();
      delete session;
      return FAILED;
    }
    ```

8. （可选）加载Graph到创建的Stream上。

    如果不调用LoadGraph接口，RunGraphWithStreamAsync接口将自动调用LoadGraph以完成加载。LoadGraph中的options可以加载配置参数，比如可以加载**“ge.exec.frozenInputIndexes（设置地址不刷新的输入Tensor索引）”**参数，该配置参数可以提升图执行性能；参数说明请参见[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/basic_functions.md)。

    ```c++
    std::map <AscendString, AscendString> options;
    uint32_t graph_id = 0;
    ret = session->LoadGraph(graph_id, options, stream);
    if(ret != SUCCESS) {
      // ...
      // ...
      // 释放资源
      ge::GEFinalizeV2();
      delete session;
      return FAILED;
    }
    ```

9. 数据传输。

    ```c++
    // 内存复制，将Host上数据传输到Device
    // hostPtrA表示Host上源内存地址指针，devPtrB表示Device上目的内存地址指针，size表示内存大小
    aclrtMemcpy(devPtrB, size, hostPtrA, size, ACL_MEMCPY_HOST_TO_DEVICE);
    ```

10. 异步执行Graph，输出执行结果。

    ```c++
    ret = session->RunGraphWithStreamAsync(graph_id, stream, input, output);

    // 调用aclrtSynchronizeStream接口，阻塞应用程序运行，直到指定Stream中的所有任务都完成
    aclRet = aclrtSynchronizeStream(stream);

    // 内存复制，将Device数据传回Host
    // devPtrA表示Device上的源内存地址指针，hostPtrB表示Host上的目的内存地址指针，size表示内存大小
    aclrtMemcpy(hostPtrB, size, devPtrA, size, ACL_MEMCPY_DEVICE_TO_HOST);

    ```

11. 释放资源。

    ```c++
    // 释放内存
    ret = aclrtFree(devPtrB);
    ret = aclrtFreeHost(hostPtrA);

    // 释放Graph资源
    ret = ge::GEFinalizeV2();

    // acl去初始化
    ret = aclFinalize();
    ```

## 单进程多卡异步运行

本章节介绍如何使用单进程管理多卡，即一个进程可以并发执行在不同的Device上。

### 功能介绍

涉及的主要接口为：

![图示](../figures/single_thread.png)

1. 调用[GEInitializeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEInitializeV2.md)进行系统初始化（也可在Graph构建前调用），申请系统资源。
2. 调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclinit)中的“初始化和去初始化 \> aclInit”接口，初始化acl。
3. 调用[Session构造函数](../../../api/graph_engine_api/cpp/ge/GeSession/GESession.md)创建多个Session类对象，申请Session资源，每个Session传入不同的ge.session\_device\_id，将模型运行在不同的Device。
4. 创建多个线程，每个线程传入不同的Session，下面以一个线程为例，描述简单的流程：
    1. 调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiacldevice)中的“Device管理 \> aclrtSetDevice”指定运行的Device，调用“aclrtCreateStream”创建Stream，然后调用[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclmalloc)中的“内存管理 \> aclrtMalloc”申请Device内存。
    2. 调用[AddGraph](../../../api/graph_engine_api/cpp/ge/GeSession/AddGraph.md)在Session类对象中添加定义好的图。
    3. 调用[CompileGraph](../../../api/graph_engine_api/cpp/ge/GeSession/CompileGraph.md)完成图编译。
    4. 调用[LoadGraph](../../../api/graph_engine_api/cpp/ge/GeSession/LoadGraph.md)（异步执行Graph场景），将图模型加载到前面创建的Stream上。
    5. 调用《Runtime运行时 API》中的“内存管理 \> aclrtMemcpy”将数据从Host传输到Device。
    6. 调用[RunGraphWithStreamAsync](../../../api/graph_engine_api/cpp/ge/GeSession/RunGraphWithStreamAsync.md)异步执行接口，运行Graph。
    7. 调用“aclrtSynchronizeStream”阻塞程序运行，直到指定Stream中的所有任务都完成。
    8. 调用《Runtime运行时 API》中的“内存管理 \> aclrtMemcpy”将数据从Device回传到Host。
    9. 调用《Runtime运行时 API》中的“内存管理 \> aclrtFree”释放内存。

5. 调用[GEFinalizeV2](../../../api/graph_engine_api/cpp/ge/GeSession/GEFinalizeV2.md)，释放系统资源；调用《Runtime运行时 API》中的“初始化和去初始化 \> aclFinalize”释放相关资源。

### 开发示例

1. 包含的头文件，包括acl、C或C++标准库的头文件。

    ```c++
    #include "ge_api_v2.h"
    #include "acl.h"
    #include "acl_rt.h"
    #include "graph/ascend_string.h"
    #include <thread>
    ```

2. 申请系统资源。

    Graph定义完成后，调用GEInitializeV2进行系统初始化（也可在Graph定义前调用），申请系统资源。示例代码如下：

    ```c++
    std::map<AscendString, AscendString>config = {{"ge.exec.deviceId", "0"},
                                                  {"ge.graphRunMode", "1"}};
    Status ret = ge::GEInitializeV2(config);
    ```

    可以通过config配置传入GE运行的初始化信息，配置参数ge.exec.deviceId和ge.graphRunMode，分别用于指定GE实例运行设备，图执行模式（在线推理请配置为0，训练请配置为1）。更多配置请参考[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/basic_functions.md)。

    > [!NOTE]说明
    >
    >GE options中的dump信息，与后续调用acl初始化接口时配置的dump信息，建议两者不要同时配置，否则可能导致异常。其他相同功能的参数类似。

3. acl资源初始化。

    ```c++
    std::string aclConfigPath = "xx/xx/xx";
    aclError retInit = aclInit(aclConfigPath);
    if (retInit != ACL_ERROR_NONE) {
        // ...
        // ...
        return FAILED;
    }
    ```

4. 创建多个Session。

    若想使定义好的Graph运行起来，首先，要创建一个Session对象。Session中的options可以加载配置参数，支持的配置参数请参见[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/options_parameters_description.md)。

    ```c++
    int thread_num = 8; // 以8个device为例
    for (int i= 0; i < thread_num; ++i) {   // 创建多个Session，每个Session的options中，传入不同的ge.session_device_id
        std::map<ge::AscendString, ge::AscendString> options = { // 构造Session的配置
     {"ge.session_device_id",std::to_string(i).c_str()},
        };
        ge::GeSession *session = new ge::GeSession(options); // 创建Session，传入配置map
        if (session == nullptr) { // 检查Session是否创建成功
     std::cout << "create session failed!" << std::endl;
     ge::GEFinalizeV2();
     return FAILED;
        }
        sessions.push_back(session);
    }
    ```

5. 创建多个线程，每个线程传入不同的Session和ge.session\_device\_id，进行异步运行Graph。

    ```c++
    //  用来保存所有线程对象的容器
    std::vector<std::thread> threads;
    // 创建多条线程并存入容器
    for (int i= 0; i < thread_num; i++) {
        std::thread worker_thread(exec_func, i); // 创建一个线程，执行exec_func，exec_func线程函数如5.a~5.g步骤所示
        threads.emplace_back(std::move(worker_thread)); //通过std::move将worker_thread移动到threads容器中
    }
    // 等待所有线程完成
    for (int i = 0; i < thread_num; i++) {
        threads.at(i).join();
    }
    ```

    单个线程exec\_func异步运行步骤如下：

    1. 指定运行的Device，创建Stream，申请内存。

        ```c++
        // 指定用于运算的Device
        int32_t deviceId = 0;
        retInit = aclrtSetDevice(deviceId);

        // 创建一个Stream
        aclrtStream stream = nullptr;
        aclError aclRet = aclrtCreateStreamWithConfig(&stream, 0, ACL_STREAM_FAST_LAUNCH);

        // 申请Device上的内存
        void* devPtrB = NULL;
        aclRet = aclrtMalloc(&devPtrB, data_size, ACL_MEM_MALLOC_HUGE_FIRST);
        ```

    2. 添加Graph对象。

        ```c++
        uint32_t graph_id = 0;
        ge::Graph graph;
        sess_ = sessionList[index];
        ge::Status ret = sess_ -> AddGraph(graph_id, graph, graph_options);
        if(ret != SUCCESS) {
          // ...
          // ...
          // 释放资源
          ge::GEFinalizeV2();
          delete session;
          return FAILED;
        }
        ```

        用户可以通过传入options配置图运行相关配置信息，其中图运行完之后的数据保存在Tensor output\_cov中。

    3. （可选）编译Graph。

        如果不调用CompileGraph接口，LoadGraph接口将自动调用CompileGraph以完成编译。

        ```c++
        uint32_t graph_id = 0;
        ret = sess_ -> CompileGraph(graph_id);
        if(ret != SUCCESS) {
          // ...
          // ...
          // 释放资源并销毁Session
          ge::GEFinalizeV2();
          delete session;
          return FAILED;
        }
        ```

    4. （可选）加载Graph到创建的Stream上。

        如果不调用LoadGraph接口，RunGraphWithStreamAsync接口将自动调用LoadGraph以完成加载。LoadGraph中的options可以加载配置参数，支持的配置参数请参见[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/options_parameters_description.md)。

        ```c++
        std::map <AscendString, AscendString> options;
        uint32_t graph_id = 0;
        ret = sess_ -> LoadGraph(graph_id, options, stream);
        if(ret != SUCCESS) {
          // ...
          // ...
          // 释放资源并销毁Session
          ge::GEFinalizeV2();
          delete session;
          return FAILED;
        }
        ```

    5. 数据传输。

        ```c++
        // 内存复制，将Host上数据传输到Device
        // hostPtrA表示Host上源内存地址指针，devPtrB表示Device上目的内存地址指针，size表示内存大小
        aclrtMemcpy(devPtrB, size, hostPtrA, size, ACL_MEMCPY_HOST_TO_DEVICE);
        ```

    6. 异步运行Graph，输出执行结果。

        ```c++
        std::vector<gert::Tensor> input;
        std::vector<gert::Tensor> output;
        ret = sess_->RunGraphWithStreamAsync(graph_id, stream, input, output);

        // 调用aclrtSynchronizeStream接口，阻塞应用程序运行，直到指定Stream中的所有任务都完成
        aclRet = aclrtSynchronizeStream(stream);
        // 内存复制，将Device数据传回Host
        // devPtrA表示Device上源内存地址指针，hostPtrB表示Host上目的内存地址指针，size表示内存大小
        aclrtMemcpy(hostPtrB, size, devPtrA, size, ACL_MEMCPY_DEVICE_TO_HOST);
        ```

    7. 释放内存。

        ```c++
        // 释放内存
        ret = aclrtFree(devPtrB);
        ```

6. 释放资源。

    ```c++
    // 释放各个Session资源
    for (auto session : sessions) {
        delete session;
    }

    // 释放Graph资源
    ret = ge::GEFinalizeV2();

    // acl去初始化
    ret = aclFinalize();
    ```
