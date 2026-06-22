# LLMException

调用LLMDataDist各接口，异常场景可能抛出LLMException异常。当前该类下只有一个接口status\_code。

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品：支持
- Atlas A2 训练系列产品：不支持

## 函数功能

获取异常的error-code。error-code列表详见[LLMStatusCode](LLMStatusCode.md)。

## 函数原型

```python
status_code()
```

## 参数说明

无

## 调用示例

```python
from llm_datadist import *
...
cache_keys = [CacheKey(1, req_id=1), CacheKey(1, req_id=2)]
try:
    kv_cache_manager.pull_cache(cache_keys[0], cache, 0)
except LLMException as exe:
    print(exe.status_code)
```

## 返回值

返回error-code。

## 约束说明

无
