# kv\_cache\_manager

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品：支持
- Atlas A2 训练系列产品：不支持

## 函数功能

获取KvCacheManager实例。

## 函数原型

```python
kv_cache_manager()
```

## 参数说明

无

## 调用示例

```python
from llm_datadist import LLMDataDist, LLMRole
llm_datadist = LLMDataDist(LLMRole.DECODER, 0)
...
llm_datadist.init(engine_options)
kv_cache_manager = llm_datadist.kv_cache_manager
```

## 返回值

返回KvCacheManager实例。

## 约束说明

无
