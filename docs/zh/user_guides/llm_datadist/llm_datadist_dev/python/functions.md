# 功能介绍

## 链路管理

**功能介绍**

NN模型执行时调用的HCCL集合通信接口是双边通信，即需要两边同时发起建链，而在P-D分离方案中，简化建链操作，由Client单侧发起建链。由于动态扩缩的部分大多数是Decode侧，因此将P定义为Server端，D定义为Client端，建链过程实现由D向P发起建链的流程。

主要提供的是link\_clusters和unlink\_clusters两个接口，都是由D侧进行调用，建链行为是点对点的。

- link\_clusters用于节点之间的建链
- unlink\_clusters用于节点之间的断链

**使用场景**

- 建链操作是PD之间进行KV Cache传输的前提，所以使能KV Cache传输功能前需要先建链。
- 集群可靠性场景下，当P或者D集群节点出现异常时，在不影响整个集群可用性前提下，通过断链下线对应故障节点。
- 通过建链和断链动态调整PD集群配比。根据闲忙，动态的增加或减少对应的机器节点。增加节点需要建链，减少节点需要断链。

**功能示例**

此处代码示例为1P1D之间建链的伪代码流程。

1. 拉起P和D侧脚本，脚本中调用LLM-DataDist的初始化接口。P侧需要设置侦听的Device IP和port。

    ```python
    # P侧脚本
    from llm_datadist import LLMDataDist, LLMRole, LLMStatusCode, LLMClusterInfo

     # llm datadist初始化
     llm_datadist = LLMDataDist(LLMRole.Prompt, cluster_id=0)
     llm_config = LLMConfig()
     llm_config.listen_ip_info ="192.168.1.1:26000"# local_ip + port
     llm_config.device_id =0  # 此处的device_id需要和local_ip匹配
     llm_options = llm_config.generate_options()
     llm_datadist.init(llm_options)
    ```

    ```python
    # D侧脚本
    from llm_datadist import LLMDataDist, LLMRole, LLMStatusCode, LLMClusterInfo

     # llm datadist初始化
     llm_datadist = LLMDataDist(LLMRole.DECODER, cluster_id=0)
     llm_config = LLMConfig()
     llm_config.device_id =0
     llm_options = llm_config.generate_options()
     llm_datadist.init(llm_options)
    ```

2. 在D侧脚本中调用link\_clusters发起建链操作，当业务退出时在D侧调用unlink\_clusters进行断链。

    ```python
     # 生成cluster info信息用于建链
     cluster = LLMClusterInfo()
     cluster.remote_cluster_id = 1  # 此处的remote_cluster_id需要和P侧创建的LLMDataDist对应
     cluster.append_local_ip_info("192.168.2.1", 26000) # local_ip_info的IP是本机需要建链的Device IP地址
     cluster.append_remote_ip_info("192.168.1.1", 26000) # remote_ip_info的IP是想和对端建链的Device IP地址

     # 调用link_clusters进行建链
     # ret是接口的返回值，rets表示每个cluster建链的结果
     ret, rets = llm_datadist.link_clusters([cluster], timeout=5000)
     # 判断建链结果
     if ret != LLMStatusCode.LLM_SUCCESS:
         raise Exception("link failed.")
     for cluster_i in range(len(rets)):
         link_ret = rets[cluster_i]
         if link_ret != LLMStatusCode.LLM_SUCCESS:
             print(f"{cluster_i} link failed.")
    ```

3. 在D侧可以调用check\_link\_status快速检查链路传输数据是否正常。

    ```python
    try:
        llm_datadist.check_link_status(remote_cluster_id=1)
    except LLMException as ex:
        print(f"check_link_status exception:{ex.status_code}")
        raise ex
    ```

4. 业务结束D侧进行断链，P和D都调用llm\_datadist的finalize释放资源。

    ```python
     # P侧脚本
     # 调用llm_datadist申请KV Cache
     # 执行业务推理
     # ...

     # 业务退出，需等D侧断链后调用
     llm_datadist.finalize()
    ```

    ```python
     # D侧脚本
     # pull_cache、模型推理
     # ...

     # 业务退出，调用unlink_clusters进行断链
     ret, rets = llm_datadist.unlink_clusters([cluster], timeout=5000)
     if ret != LLMStatusCode.LLM_SUCCESS:
         raise RuntimeError(f'[unlink_cluster] failed, ret={ret}')
     llm_datadist.finalize()
    ```

当新增节点或者已下线节点再上线时，需要执行一遍上述使用流程。当下线节点时，正常情况下D侧需要主动调用unlink\_clusters接口_，_如果D侧无法调用unlink\_clusters接口，则需要P侧调用unlink\_clusters接口。

通过节点上线调用link\_clusters接口，节点下线调用unlink\_clusters接口来灵活的进行分布式集群的动态扩缩容。

**异常处理**

当D侧出现异常导致无法调用unlink\_clusters时，需要由P侧调用unlink\_clusters进行资源清理，否则无法再次进行建链。

unlink\_clusters接口提供了强制断链的能力，该能力适用于链路故障时，普通断链操作会耗时比较久。使用强制断链接口（设置force=True），需要两侧都发起调用，只会清理本端的链接。

```python
# 强制断链
 ret, rets = llm_datadist.unlink_clusters([cluster], timeout=5000, force=True)
 if ret != LLMStatusCode.LLM_SUCCESS:
     raise RuntimeError(f'[unlink_clusters] failed, ret={ret}')
```

## KV Cache管理

**功能介绍**

在LLM-DataDist初始化时会预申请一块指定大小的内存池，由ge.flowGraphMemMaxSize配置项决定其大小，后续的KV Cache的内存申请及释放都在预申请的内存上进行，相比每次申请一块内存，可以节省耗时。

KV Cache管理涉及的主要接口及功能如下：

**表1**  KV Cache管理的主要接口及功能

|接口名称|功能|
|--|--|
|allocate_cache|分配cache。cache分配成功后，会同时被cache_id与cache_keys引用，只有当这些引用都解除后，cache所占用的资源才会实际释放。cache_keys会在LLMRole为PROMPT时传入。|
|deallocate_cache|释放cache。如果该cache在分配内存时关联了cache_keys，则实际的释放会延后到所有的cache_keys都已经释放，cache_keys的引用则可以通过以下2种方式解除：Decode调用pull_cache接口成功后解除。Prompt调用remove_cache_key接口时解除。|
|remove_cache_key|移除cache_key。移除cache_key后，该cache将无法再被pull_cache拉取。仅当LLMRole为PROMPT时可调用，LLMRole为DECODER时因为allocate_cache申请时不需要传入cache_key，所以不需要调用。|
|pull_cache|根据CacheKey，从对应的Prompt节点拉取KV到本地KV Cache，仅当LLMRole为DECODER时可调用。该CacheKey需要和allocate_cache的CacheKey保持一致。|
|copy_cache|拷贝KV Cache。当期望pull_cache和其他使用KV Cache的操作流水时，可以额外申请一块中转cache。当其他流程在使用KV Cache时，可以先将下一次的KV pull到中转cache，待其他流程使用完KV Cache后，拷贝到指定的位置，从而将pull_cache的耗时隐藏，减少总耗时。公共前缀场景在新请求推理前可以将公共前缀拷贝到新的内存中和当前请求的KV合并推理。|
|allocate_blocks_cache|PagedAttention场景下，分配多个blocks的Cache。|
|pull_blocks|PA场景下，KV的拉取接口。和pull_cache的差异是，pull_blocks是按照block_index拉取的对应位置的KV Cache。|
|copy_blocks|PA场景下，KV的拷贝接口。和copy_cache的差异是，copy_blocks是按照block_index拷贝的对应位置的KV Cache。在使用场景上也存在差异，copy_blocks主要是针对当多个回答需要共用相同block，block没填满时，新增的token需要拷贝到新的block上继续迭代。|
|swap_blocks|PA场景下，对应block_index上KV内存的换入换出，主要面向用户需要自行管理KV内存的场景。|
|transfer_cache_async|由Prompt节点发起，将cache的数据传输到Decode。|
|push_blocks|PA场景下，KV的推送接口，从对应的Prompt节点推送KV到远端KV Cache。|
|push_cache|从对应的Prompt节点推送KV到远端KV Cache。|

**使用场景**

主要用于分布式集群间的KV Cache传输。

**功能示例（一般Cache传输场景）**

本示例介绍一般Cache传输场景下接口的使用，主要涉及KV Cache的申请、释放、传输。如下将根据业务角色给出伪代码示例。

1. P侧和D侧根据[链路管理](functions.md#链路管理)章节的示例完成LLMDataDist的初始化和建链操作。
2. 在P侧给每个请求申请对应大小的KV Cache内存，以torch为例，将KV Cache转换为torch tensor，进行全量模型推理。

    ```python
    import torchair
    import torch
    import torch_npu
    # 从初始化完的llm_datadist中获取kv_cache_manager
    kv_cache_manager = llm_datadist.kv_cache_manager
    # 根据模型中KV Cache的shape以及总个数创建CacheDesc。此处shape只是示例，实际填写网络中的kv cache shape。
    cache_desc = CacheDesc(num_tensors=4, shape=[4, 4, 8], data_type=DataType.DT_FLOAT16)
    # 根据初始化llm_datadist时的cluster_id创建对应请求的cache_key，当P侧是多batch模型时，需要创建batch数量的cache_key
    batch_num = 8
    kv_cache = kv_cache_manager.allocate_cache(cache_desc, [CacheKey(prompt_cluster_id=0, req_id=i, model_id=0) for i in range(batch_num)])

    # 将申请好的KV Cache转换为torch tensor
    kv_tensor_addrs = kv_cache.per_device_tensor_addrs[0]
    kv_tensors = torchair.llm_datadist.create_npu_tensors(kv_cache.cache_desc.shape, torch.float16, kv_tensor_addrs)
    # 将转换的kv_tensors传给模型推理计算产生KV Cache，将模型输出传输给增量推理模型作为输入
    ```

3. 在D侧申请一份用于模型执行的KV Cache内存。

    ```python
    # 从初始化完的llm_datadist中获取kv_cache_manager
    kv_cache_manager = llm_datadist.kv_cache_manager
    # 根据模型中KV Cache的shape以及总个数创建CacheDesc
    cache_desc = CacheDesc(num_tensors=4, shape=[4, 4, 8], data_type=DataType.DT_FLOAT16)
    # 调用allocate_cache接口申请对应请求的KV Cache内存
    kv_cache = kv_cache_manager.allocate_cache(cache_desc)
    ```

4. 将KV Cache从P侧传输到D侧，有以下两种方式：
    - 在D侧调用pull\_cache接口拉取对应请求的KV Cache到申请的内存中。

        ```python
        # 创建和P侧申请cache时相同的cache_key，用于拉取对应的KV Cache
        cache_key = CacheKey(prompt_cluster_id=0, req_id=1, model_id=0)
        kv_cache_manager.pull_cache(cache_key, kv_cache, batch_index=1) # 拉到batch index为1的位置上
        ```

    - 在P侧调用transfer\_cache\_async接口将数据传输至Decode。

        ```python
        from llm_datadist import LayerSynchronizer, TransferConfig


        class LayerSynchronizerImpl(LayerSynchronizer):
            def __init__(self, events):
                self._events = events

            def synchronize_layer(self, layer_index: int, timeout_in_millis: Optional[int]) -> bool:
                self._events[layer_index].wait()
                return True


        events = [torch.npu.Event() for _ in range(cache_desc.num_tensors // 2)]
        # 执行模型，模型在各层计算完成后调用events[layer_index].record()记录完成状态
        # 模型执行由用户实现
        # user_model.Predict(kv_tensors, events)

        # 模型下发完成后，调用transfer_cache_async传输数据，此处需要填写Decode已申请的KV Cache各层tensor的内存地址
        transfer_config = TransferConfig(DECODER_CLUSTER_ID, decoder_kv_cache_addrs)
        cache_task = kv_cache_manager.transfer_cache_async(kv_cache, LayerSynchronizerImpl(events), [transfer_config])
        # 同步等待传输结果
        cache_task.synchronize()
        ```

5. 以torch为例，将KV Cache转换为torch tensor，进行增量模型推理。

    ```python
    # 将申请好的KV Cache转换为框架中的KV Cache类型，不同框架中都需要提供根据KV Cache地址创建对应类型的KV Cache的接口。此处以PyTorch为例
    # 转换操作和pull操作顺序不分先后
    kv_tensor_addrs = kv_cache.per_device_tensor_addrs[0]
    kv_tensors = torchair.llm_datadist.create_npu_tensors(kv_cache.cache_desc.shape, torch.float16, kv_tensor_addrs)
    # 将转换后的tensor拆分为框架需要的KV配对方式，可以自定义组合KV
    mid = len(kv_tensors) // 2
    k_tensors = kv_tensors[: mid]
    v_tensors = kv_tensors[mid:]
    kv_cache_tensors = list(zip(k_tensors, v_tensors))

    # 将转换的kv_tensors传给模型进行迭代推理
    # 等待请求增量推理完成
    ```

6. 根据业务中cache的使用时机自行释放对应请求的KV Cache内存。

    ```python
    # P侧：当batch中所有请求的KV Cache都被拉走后，调用deallocate_cache才会真正释放内存。如果D侧未拉走KV Cache，则还需要调用remove_cache_key。
    kv_cache_manager.remove_cache_key(cache_key_0)
    kv_cache_manager.remove_cache_key(cache_key_2)
    kv_cache_manager.deallocate_cache(kv_cache)
    ```

    ```python
    # D侧：由于申请时不需要cache_key，所以释放时只需要调用deallocate接口。
    kv_cache_manager.deallocate_cache(kv_cache)
    ```

7. 业务退出时，P侧和D侧根据[链路管理](functions.md#链路管理)章节的示例进行断链，然后调用finalize接口释放资源。

**功能示例（Blocks Cache传输场景）**

本示例介绍Blocks Cache（将Cache使用块状形式管理）传输场景下接口的使用，主要涉及KV Cache的申请、释放、传输。如下将根据业务角色给出伪代码示例。

1. P侧和D侧根据[链路管理](functions.md#链路管理)的示例完成LLMDataDist的初始化和建链操作。
2. 在P侧和D侧模型的每层按照计算好的num\_blocks数量调用AllocateCache申请KV Cache。Blocks Cache场景下，不同请求对创建的num\_blocks大小的KV Cache进行复用，上层框架自行管理，业务结束后释放申请的内存。

    ```python
    # P侧
    # 从初始化完的llm_datadist中获取kv_cache_manager
    kv_cache_manager = llm_datadist.kv_cache_manager
    # 根据模型中KV Cache的shape以及总个数创建CacheDesc。PA场景的kv cache shape通常为[num_blocks, block_size,...,...]
    num_blocks = 10
    block_mem_size = 128
    cache_desc = CacheDesc(num_tensors=4, shape=[num_blocks, block_mem_size], data_type=DataType.DT_FLOAT16)
    # 根据初始化llm_datadist时的cluster_id创建对应请求的BlocksCacheKey
    cache_key = BlocksCacheKey(prompt_cluster_id=0, model_id=0)
    # 调用allocate_blocks_cache接口申请KV Cache内存
    kv_cache = kv_cache_manager.allocate_blocks_cache(cache_desc, cache_key)

    # 将申请好的KV Cache转换为框架中的KV Cache类型，不同框架中需要提供根据KV Cache的地址创建对应类型的KV Cache的接口。此处以PyTorch为例
    kv_tensor_addrs = kv_cache.per_device_tensor_addrs[0]
    kv_tensors = torchair.llm_datadist.create_npu_tensors(kv_cache.cache_desc.shape, torch.float16, kv_tensor_addrs)
    ```

    ```python
    # D侧
    kv_cache_manager = llm_datadist.kv_cache_manager
    num_blocks = 10
    block_mem_size = 128
    cache_desc = CacheDesc(num_tensors=4, shape=[10, 128], data_type=DataType.DT_FLOAT16)
    kv_cache = kv_cache_manager.allocate_blocks_cache(cache_desc)

    kv_tensor_addrs = kv_cache.per_device_tensor_addrs[0]
    kv_tensors = torchair.llm_datadist.create_npu_tensors(kv_cache.cache_desc.shape, torch.float16, kv_tensor_addrs)
    ```

3. P侧有新请求进来后，推理框架会给每个请求分配好对应的block\_index。模型推理完之后，该请求对应的KV Cache就在对应的block\_index所在的内存上，将模型输出和请求对应的block\_table传输给D侧推理模型作为输入。
4. D侧有新请求进来后，推理框架也会给每个请求分配好对应的block\_index，然后调用pull\_blocks接口，根据P侧的block\_index和D侧的block\_index的对应关系，将KV Cache传输到指定位置。此时有两种方式：
    - 在D侧调用pull\_blocks接口拉取KV Cache。

        ```python
        # D侧根据P侧传过来的信息，添加新请求，并申请对应的block_table
        # D侧根据传过来请求的src_block_table和新申请的dst_block_table拉取KV到对应block
        cache_key = BlocksCacheKey(prompt_cluster_id=0, model_id=0) # P侧allocate_blocks_cache时的入参
        kv_cache_manager.pull_blocks(cache_key, cache, [0, 1], [2, 3]) # 将P侧0, 1 block位置上的数据拉到D侧2, 3 block位置上
        ```

    - 在P侧调用transfer\_cache\_async接口时将数据传输至D侧。

        ```python
        # 实现LayerSynchronizerImpl，通过torch Event获取各层计算结束状态，本例中通过Event机制实现
        class LayerSynchronizerImpl(LayerSynchronizer):
            def __init__(self, events):
                self._events = events

            def synchronize_layer(self, layer_index: int, timeout_in_millis: Optional[int]) -> bool:
                self._events[layer_index].wait()
                return True


        events = [torch.npu.Event() for _ in range(cache_desc.num_tensors // 2)]
        # 执行模型,模型在各层计算完成后调用events[layer_index].record()记录完成状态
        # 该函数由用户实现
        user_model.Predict(kv_cache_tensors, events)

        # 模型下发完成后，调用transfer_cache_async传输数据，此处需要填写Decode申请出的KV Cache各层tensor的内存地址
        transfer_config = TransferConfig(DECODER_CLUSTER_ID, decoder_kv_cache_addrs)
        cache_task = kv_cache_manager.transfer_cache_async(kv_cache, LayerSynchronizerImpl(events), [transfer_config], [0, 1],
                                                           [2, 3])
        # 同步等待传输结果
        cache_task.synchronize()
        ```

5. 业务结束后P侧和D侧调用deallocate\_cache释放已申请的KV Cache内存。

    ```python
    # 等待D侧拉取完对应请求的KV Cache
    # 根据业务中cache的使用时机自行释放对应请求的KV Cache。PA场景无需释放对应的cache_key
    kv_cache_manager.deallocate_cache(kv_cache)
    ```

6. 业务退出时，P侧和D侧根据集群断链的示例进行断链和llm\_datadist的finalize。

**异常处理**

- 错误码LLM\_DEVICE\_OUT\_OF\_MEMORY表示Device申请KV Cache内存失败。需要检查初始化时设置的ge.flowGraphMemMaxSize大小以及申请KV Cache的大小，查看是否有请求KV Cache拉取之后没有释放内存。
- 错误码LLM\_KV\_CACHE\_NOT\_EXIST表示对端KV Cache不存在，需要检查对端进程是否异常或者对应KV Cache的请求有没有推理完成。该错误不影响其他请求流程，确认流程后可以重试。
- 错误码LLM\_WAIT\_PROCESS\_TIMEOUT表示pull KV超时，说明链路出现问题，需要重新断链建链再尝试。
