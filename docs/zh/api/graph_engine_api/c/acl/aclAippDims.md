# aclAippDims

```c
typedef struct aclAippDims {
    aclmdlIODims srcDims;     // 模型转换前的dims，如[1,3,150,150]
    size_t srcSize;           // 模型转换前输入数据的大小
    aclmdlIODims aippOutdims; // 内部数据格式的dims信息
    size_t aippOutSize;       // 经过AIPP处理后的输出数据的大小
} aclAippDims;
```

aclmdlIODims的定义请参见[aclmdlIODims](aclmdlIODims.md)。
