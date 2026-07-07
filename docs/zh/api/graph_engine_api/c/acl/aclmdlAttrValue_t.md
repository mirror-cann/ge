# aclmdlAttrValue_t

```c
typedef union {
    uint32_t mdlPriority; // 模型执行的优先级，数字越小优先级越高，取值[0,7]，默认值为0
    uint32_t reserved[8]; // 预留
} aclmdlAttrValue_t;
```
