# 专题

## 角色切换

**使用场景**

主要用于在PD集群节点数量固定的场景下，由于业务的变化，期望PD集群节点间可以相互切换，充分利用资源。

**涉及的接口**

调用SetRole接口对LLM-DataDist的角色进行切换。

**功能示例**

示例由角色Decoder切换为Prompt。切换角色后，根据业务功能调用LLM-DataDist其他接口。

1. LLM-DataDist初始化时，需要设置OPTION\_ENABLE\_SET\_ROLE为"1"。

    ```python
    LlmDataDist llm_data_dist(DECODER_CLUSTER_ID, LlmRole::kDecoder);
    std::map<AscendString, AscendString> options;
    options[OPTION_DEVICE_ID] = "0";
    options[OPTION_BUF_POOL_CFG] = R"({
    "buf_pool_size": 2147483648
    })";
    options[OPTION_ENABLE_SET_ROLE] = "1";
    auto ret = llm_data_dist.Initialize(options);
    if (ret != LLM_SUCCESS) {
        printf("[ERROR] Initialize failed, ret = %u\n", ret);
        return -1;
    }
    ```

2. 调用SetRole进行角色切换。

    ```python
    std::map<AscendString, AscendString> options;
    //替换为真实IP端口
    options[OPTION_LISTEN_IP_INFO] = "ip:port";
    llmDataDist.SetRole(LlmRole::kPrompt, options);
    ```

**异常处理**

异常处理请参考错误码。

## 公共前缀

**使用场景**

公共前缀指的是模型推理的多个输入包含相同的起始部分。

该功能可用于将公共前缀产生的KV Cache内存拷贝到新的用户请求的KV Cache上进行推理。

**涉及接口**

|接口名称|功能|
|--|--|
|CopyKvCache|拷贝KV Cache。|

**功能示例**

```python
ret = llmDataDist.CopyKvCache(src_cache, dst_cache, 0, 0);
if (ret != LLM_SUCCESS) {
    printf("[ERROR] CopyKvCache failed, ret = %u\n", ret);
    return -1;
}
```

**异常处理**

异常处理请参考错误码。
