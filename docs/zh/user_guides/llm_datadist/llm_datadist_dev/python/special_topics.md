# 专题

## KV Cache换入换出

**功能介绍**

KV Cache换入换出指将KV Cache从Device换出到Host，或者从Host换入Device。

在PA场景下，由于KV Cache的容量有限，需要根据任务优先级，执行状态等因素，动态地将任务所需的KV Cache进行换入换出，提高显存利用率。

**涉及的接口**

|接口名称|功能|
|--|--|
|CacheManager.swap_blocks|CacheManager场景下，将对应block_index上的KV内存换入换出|
|KvCacheManager.swap_blocks|KvCacheManager场景下，将对应block_index上的KV内存换入换出|

**功能示例**

```python
from llm_datadist import KVCache
...
npu_cache = kv_cache_manager.allocate_blocks_cache(npu_cache_desc, npu_cache_key)
cpu_cache = KvCache.create_cpu_cache(cpu_cache_desc, cpu_addrs) # cpu_addrs来自创建的cpu tensors
# swap in
kv_cache_manager.swap_blocks(cpu_cache, npu_cache, {1:2, 3:4})
# swap out
kv_cache_manager.swap_blocks(npu_cache, cpu_cache, {1:2, 3:4})
```

## 公共前缀

**功能介绍**

公共前缀指的是在一次推理过程中，多个输入提示包含相同的起始部分。

可用于将公共前缀产生的KV Cache内存拷贝到新的用户请求的KV Cache上进行推理。

**涉及的接口**

|接口名称|功能|
|--|--|
|CacheManager.copy_cache|CacheManager场景下，拷贝cache。|
|KvCacheManager.copy_cache|KvCacheManager场景下，拷贝cache。|

**功能示例**

```python
src_cache = kv_cache_manager.allocate_cache(npu_cache_desc, npu_cache_key) # 前缀cache
dst_cache = kv_cache_manager.allocate_cache(npu_cache_desc, npu_cache_key) # 新的请求cache

kv_cache_manager.copy_cache(dst_cache, src_cache, dst_batch_index, src_batch_index, offset, size)
```
