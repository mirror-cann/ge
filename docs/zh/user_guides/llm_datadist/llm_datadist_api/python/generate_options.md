# generate\_options

## 产品支持情况

- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 推理系列产品：支持
- Atlas A2 训练系列产品：不支持

## 函数功能

生成配置项字典。

## 函数原型

```python
generate_options()
```

## 参数说明

无

## 调用示例

```python
from llm_datadist import LLMConfig
llm_config = LLMConfig()
...
engine_options = llm_config.generate_options()
```

## 返回值

返回配置项字典。

## 约束说明

无
