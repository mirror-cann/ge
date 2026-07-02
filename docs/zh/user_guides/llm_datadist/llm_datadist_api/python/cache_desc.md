﻿# cache\_desc

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品：支持
- Atlas A2 训练系列产品：不支持

## 函数功能

获取KvCache描述。

## 函数原型

```python
@property
cache_desc() -> CacheDesc
```

## 参数说明

无

## 调用示例

```python
...
kv_cache = kv_cache_manager.allocate_cache(cache_desc, cache_keys)
print(kv_cache.cache_desc.num_tensors)
```

## 返回值

正常情况返回类型为KvCache的cache描述。

## 约束说明

无
