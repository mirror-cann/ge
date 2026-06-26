# 功能介绍

## 链路管理

### 建链

**功能介绍**

调用LinkLlmClusters接口在PD节点之间建立通信链路，链路主要用于KV Cache的传输。

**使用场景**

建链操作是节点之间进行数据传输的前提，在D2D模式中，建链接口采用类TCP建链流程，P侧初始化时提供侦听信息，由D侧发起建链。

根据业务繁忙情况，在需要调整集群PD节点配比时，通过建链扩容节点。

**功能示例**

1. 初始化LLM-DataDist。其中P侧需要设置侦听的Device IP和port。

    ```python
    // P侧
    LlmDataDist llm_data_dist(PROMPT_CLUSTER_ID, LlmRole::kPrompt);
    std::map<AscendString, AscendString> options;
    options[OPTION_DEVICE_ID] = "0";
    //替换为真实IP端口
    options[OPTION_LISTEN_IP_INFO] = "ip:port";
    options[OPTION_BUF_POOL_CFG] = R"({
    "buf_cfg":[{"total_size":2097152,"blk_size":256,"max_buf_size":8192}],
    "buf_pool_size": 2147483648
    })";
    auto ret = llm_data_dist.Initialize(options);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] Initialize failed, ret = %u\n", ret);
        return -1;
    }
    // D侧
    LlmDataDist llm_data_dist(DECODER_CLUSTER_ID, LlmRole::kDecoder);
    std::map<AscendString, AscendString> options;
    options[OPTION_DEVICE_ID] = "0";
    options[OPTION_BUF_POOL_CFG] = R"({
    "buf_pool_size": 2147483648
    })";
    auto ret = llm_data_dist.Initialize(options);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] Initialize failed, ret = %u\n", ret);
        return -1;
    }
    ```

2. 在D侧调用LinkLlmClusters接口发起建链操作。

    ```python
    std::vector<Status> rets;
    std::vector<ClusterInfo> clusters;
    ClusterInfo cluster_info;
    IpInfo local_ip_info;
    // 替换为本地真实IP
    local_ip_info.ip = "ip";
    IpInfo remote_ip_info;
    // 替换为对端真实IP
    remote_ip_info.ip = "ip";
    // 替换为P侧初始化时的侦听端口
    remote_ip_info.port = "port";
    cluster_info.remote_cluster_id = PROMPT_CLUSTER_ID;
    cluster_info.local_ip_infos.emplace_back(std::move(local_ip_info));
    cluster_info.remote_ip_infos.emplace_back(std::move(remote_ip_info));
    clusters.emplace_back(std::move(cluster_info));
    auto ret = llm_data_dist.LinkLlmClusters(clusters, rets);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] LinkLlmClusters failed, ret = %u\n", ret);
        return -1;
    }
    for (const auto &inner_ret : rets) {
        if (inner_ret != LLM_SUCCESS) {
            printf("[ERROR] LinkLlmClusters failed, ret = %u\n", inner_ret);
            return -1;
        }
    }
    ```

**异常处理**

当调用LinkLlmClusters失败时，需排查两台机器网络是否连通，Device IP是否正确，port是否被占用。

更多异常处理请参考错误码]。

### 断链

**功能介绍**

调用UnlinkLlmClusters接口断开并清理PD之间的通信链路。

**使用场景**

当P或者D集群节点出现异常时，通过断链清理异常链路，或者需要调整集群PD节点配比时，通过断链关闭已建立的链路。

**功能示例**

调用断链方式有如下两种。

一种是通过在D侧发起断链，常用在链路非故障场景。

```python
// clusters同建链的clusters
auto ret = llm_data_dist.UnlinkLlmClusters(clusters, rets);
if (ret != LLM_SUCCESS) {
    printf("[ERROR] UnlinkLlmClusters failed, ret = %u\n", ret);
    return -1;
}
for (const auto &inner_ret : rets) {
    if (inner_ret != LLM_SUCCESS) {
        printf("[ERROR] UnlinkLlmClusters failed, ret = %u\n", inner_ret);
        return -1;
    }
}
```

一种是通过在PD两侧都发起强制断链，常用在链路故障场景。

```python
// PD两侧
std::vector<Status> rets;
std::vector<ClusterInfo> clusters;
ClusterInfo cluster_info;
IpInfo local_ip_info;
// 替换为本地真实IP
local_ip_info.ip = "ip";
IpInfo remote_ip_info;
// 替换为对端真实IP
remote_ip_info.ip = "ip";
// 替换为P侧初始化时的侦听端口
remote_ip_info.port = "port";
cluster_info.remote_cluster_id = PROMPT_CLUSTER_ID;
cluster_info.local_ip_infos.emplace_back(std::move(local_ip_info)); // local_ip_infos的IP是本地的Device IP地址
cluster_info.remote_ip_infos.emplace_back(std::move(remote_ip_info)); // remote_ip_infos的IP是对端的Device IP地址
clusters.emplace_back(std::move(cluster_info));
auto ret = llmDataDist.UnlinkLlmClusters(clusters, rets, 1000, true);
if (ret != LLM_SUCCESS) {
    printf("[ERROR] UnlinkLlmClusters failed, ret = %u\n", ret);
    return -1;
}
for (const auto &inner_ret : rets) {
    if (inner_ret != LLM_SUCCESS) {
        printf("[ERROR] UnlinkLlmClusters failed, ret = %u\n", inner_ret);
        return -1;
    }
}
```

**异常处理**

当调用UnlinkLlmClusters失败时，如果是网络故障场景，可改用强制断链方式。

更多异常处理请参考错误码。

## KV Cache管理

**功能介绍**

在LLM-DataDist初始化时会预申请一块指定大小的内存池，由OPTION\_BUF\_POOL\_CFG配置项决定其大小，后续的KV Cache的内存申请及释放都在预申请的内存上进行，相比每次执行时申请一块cache内存，可以节省耗时。

KV Cache管理涉及的主要接口及功能如下：

|接口名称|功能|
|--|--|
|AllocateCache|分配Cache。|
|DeallocateCache|释放Cache。|
|PullKvCache|从远端节点拉取Cache到本地Cache，仅当角色为Decoder时可调用。|
|PullKvBlocks|在传输Blocks Cache场景下，通过配置block列表的方式，从远端节点拉取Cache到本地Cache，仅当角色为Decoder时可调用。|
|CopyKvCache|拷贝KV Cache。支持D2D，D2H的拷贝。当期望PullKvCache和其他使用Cache的操作流水时，可以额外申请一块中转Cache。当其他流程在使用Cache时，可以先将下一次的Cache pull到中转Cache，待其他流程使用完Cache后，拷贝到指定的位置，从而通过pipeline流水将PullKvCache的耗时隐藏，减少总耗时。公共前缀场景在新请求推理前，可以将公共前缀拷贝到新的内存中与当前请求的KV合并推理。|
|CopyKvBlocks|PA场景下，通过block列表的方式拷贝KV Cache。支持D2D，D2H，H2D的拷贝。D2D场景主要是针对当多个回答需要共用相同block，block没填满时，新增的token需要拷贝到新的block上继续迭代。H2D和D2H的拷贝主要用于对应block_index上Cache内存的换入换出。|
|PushKvCache|从本地节点推送Cache到远端节点，仅当角色为Prompt时可调用。|
|PushKvBlocks|在传输Blocks Cache场景下，通过配置block列表的方式，从本地节点推送Cache到远端节点，仅当角色为Prompt时可调用。|

**使用场景**

主要用于分布式集群间的KV Cache传输和搬移。

**功能示例（一般Cache传输场景）**

本示例介绍一般Cache传输场景下接口的使用，主要涉及KV Cache的申请、释放、传输。如下将根据业务角色给出伪代码示例。

1. P侧和D侧根据[建链](functions.md#建链)章节的示例完成LLM-DataDist的初始化和建链操作。
2. 在P/D侧给每个请求申请对应大小的KV Cache内存，若失败，则需要释放对应的资源。

    ```python
    void OnError(LlmDataDist &llm_data_dist, Cache &cache)
    {
        if (cache.cache_id > 0) {
            (void) llmDataDist.DeallocateCache(cache.cache_id);
        }
        llm_data_dist.Finalize();
    }
    CacheDesc kv_cache_desc{};
    kv_cache_desc.num_tensors = NUM_TENSORS;
    kv_cache_desc.data_type = DT_INT32;
    kv_cache_desc.shape = {8, 16};
    Cache cache{};
    auto ret = llm_data_dist.AllocateCache(kv_cache_desc, cache);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] AllocateCache failed, ret = %u\n", ret);
        OnError(llm_data_dist, cache);
        return -1;
    }
    ```

3. 将Cache从P侧传输到D侧，有如下两种方式
    - 在D侧调用PullKvCache接口拉取KV Cache。

        ```python
        // P侧进行全量推理写入cache，通知D侧可以pull
        // D侧拉取cache
        CacheIndex cache_index{PROMPT_CLUSTER_ID, 1, 0};
        ret = llm_data_dist.PullKvCache(cache_index, cache, 0);
        if (ret != LLM_SUCCESS) {
            printf("[ERROR] PullKvCache failed, ret = %u\n", ret);
            return -1;
        }
        // 进行增量推理
        ```

    - 在P侧调用PushKvCache接口推送KV Cache。

        ```python
        // P侧一层计算完可在传输线程立即推送，以实现将大部分传输时间和计算重叠。
        CacheIndex dst_cache_index{DECODE_CLUSTER_ID, 1, 0};
        KvCacheExtParam ext_param{};
        ext_param.src_layer_range =  std::pair<int32_t, int32_t>(0, 0);
        ext_param.dst_layer_range =  std::pair<int32_t, int32_t>(0, 0);
        // 每层tensor数量，可根据实际模型修改
        ext_param.tensor_num_per_layer = 2;
        ret = llm_data_dist.PushKvCache(cache, dst_cache_index, 0, -1, ext_param);
        if (ret != LLM_SUCCESS) {
            printf("[ERROR] PushKvCache failed, ret = %u\n", ret);
            return -1;
        }
        ```

4. P/D侧根据业务中Cache的使用时机自行释放对应请求的KV Cache内存。

    ```python
    ret = llm_data_dist.DeallocateCache(cache.cache_id);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] DeallocateCache failed, ret = %u\n", ret);
    } else {
        printf("[INFO] DeallocateCache success\n");
    }
    ```

5. 业务退出时，P侧和D侧根据[断链](functions.md#断链)章节的示例进行断链和调用finalize接口释放资源。

**功能示例（Blocks Cache传输场景）**

本示例介绍Blocks Cache（将Cache使用块状形式管理）传输场景下接口的使用，主要涉及KV Cache的申请、释放、传输。如下将根据业务角色给出伪代码示例。

1. P侧和D侧根据集群[建链](functions.md#建链)的示例完成LLM-DataDist的初始化和建链操作。
2. 在P侧和D侧模型的每层按照计算好的num\_block数量调用AllocateCache接口申请KV Cache。上层框架根据不同请求对创建的num\_block大小的KV Cache进行复用，业务结束后释放申请的内存。

    ```python
    void OnError(LlmDataDist &llm_data_dist, Cache &cache)
    {
        if (cache.cache_id > 0) {
            (void) llm_data_dist.DeallocateCache(cache.cache_id);
        }
        llm_data_dist.Finalize();
    }
    CacheDesc kv_cache_desc{};
    kv_cache_desc.num_tensors = NUM_TENSORS;
    kv_cache_desc.data_type = DT_INT32;
    kv_cache_desc.shape = {8, 16};
    Cache cache{};
    auto ret = llm_data_dist.AllocateCache(kv_cache_desc, cache);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] AllocateCache failed, ret = %u\n", ret);
        OnError(llm_data_dist, cache);
        return -1;
    }
    ```

3. P侧接收到新请求后，为每个请求分配好对应的block\_index，这部分是推理框架提供的功能。模型推理完之后，该请求对应的KV Cache就在对应的block\_index所在的内存上，将模型输出和请求对应的block\_table传输给D侧推理模型作为输入。
4. D侧接收到新请求后，为每个请求分配好对应的block\_index_，_然后调用pull\_blocks接口，根据P侧的block\_index和D侧的block\_index的对应关系，将KV Cache传输到指定位置。
    - 在D侧调用PullKvBlocks接口拉取KV Cache。

        ```python
        // P侧进行全量推理写入cache，通知D侧可以拉取cache
        // D侧拉取cache
        CacheIndex cache_index{PROMPT_CLUSTER_ID, 1, 0};
        std::vector<uint64_t> prompt_blocks = {0, 1, 2, 3};
        std::vector<uint64_t> decoder_blocks = {3, 2, 1, 0};
        auto ret = llm_data_dist.PullKvBlocks(cache_index, cache, prompt_blocks, decoder_blocks);
        if (ret != LLM_SUCCESS) {
            printf("[ERROR] PullKvBlocks failed, ret = %u\n", ret);
            return -1;
        }
        // 进行增量推理
        ```

    - 在P侧调用PushKvBlocks接口推送KV Cache。

        ```python
        // P侧一层计算完可在传输线程立即推送，以实现将大部分传输时间和计算重叠。
        CacheIndex dst_cache_index{DECODE_CLUSTER_ID, 1};
        KvCacheExtParam ext_param{};
        ext_param.src_layer_range =  std::pair<int32_t, int32_t>(0, 0);
        ext_param.dst_layer_range =  std::pair<int32_t, int32_t>(0, 0);
        // 每层tensor数量，可根据实际模型修改
        ext_param.tensor_num_per_layer = 2;
        std::vector<uint64_t> prompt_blocks = {0, 1, 2, 3};
        std::vector<uint64_t> decoder_blocks = {3, 2, 1, 0};
        ret = llm_data_dist.PushKvBlocks(cache, dst_cache_index, prompt_blocks, decoder_blocks, ext_param);
        if (ret != LLM_SUCCESS) {
            printf("[ERROR] PushKvBlocks failed, ret = %u\n", ret);
            return -1;
        }
        ```

5. P/D侧根据业务中Cache的使用时机自行释放对应请求的KV Cache内存。

    ```python
    ret = llm_data_dist.DeallocateCache(cache.cache_id);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] DeallocateCache failed, ret = %u\n", ret);
    } else {
        printf("[INFO] DeallocateCache success\n");
    }
    ```

6. 业务退出时，P侧和D侧根据[断链](functions.md#断链)章节的示例进行断链和资源释放。

**异常处理**

- 错误码LLM\_DEVICE\_OUT\_OF\_MEMORY，表示Device申请KV Cache内存失败。需要检查初始化时设置的OPTION\_BUF\_POOL\_CFG大小以及申请KV Cache的大小，查看是否有请求KV Cache拉取之后没有释放内存。
- 错误码LLM\_KV\_CACHE\_NOT\_EXIST，表示对端KV Cache不存在，需要检查对端进程是否异常或者对应KV Cache的请求是否推理完成。该错误不影响其他请求流程，确认流程后可以重试。
- 错误码LLM\_WAIT\_PROCESS\_TIMEOUT或LLM\_TIMEOUT，表示pull KV超时，说明链路出现问题，需要尝试重新断链或建链。
- 错误码LLM\_NOT\_YET\_LINK，说明与远端cluster没有建链。
