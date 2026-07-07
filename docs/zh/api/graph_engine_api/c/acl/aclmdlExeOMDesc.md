# aclmdlExeOMDesc

```c
typedef struct aclmdlExeOMDesc {
    size_t workSize;
    size_t weightSize;
    size_t modelDescSize;
    size_t kernelSize;
    size_t kernelArgsSize;
    size_t staticTaskSize;
    size_t dynamicTaskSize;
    size_t fifoTaskSize;
    size_t reserved[8];
} aclmdlExeOMDesc;
```

| 成员名称 | 描述 |
| --- | --- |
| workSize | 模型执行时所需的工作内存的大小，单位Byte。 |
| weightSize | 模型执行时所需的权值内存的大小，单位Byte。 |
| modelDescSize | 存放模型描述信息所需的内存大小，单位Byte。 |
| kernelSize | 存放TBE算子kernel（算子的*.o与*.json）所需的内存大小，单位Byte。 |
| kernelArgsSize | 存放TBE算子kernel参数所需的内存大小，单位Byte。 |
| staticTaskSize | 存放静态shape任务描述信息所需的内存大小，单位Byte。 |
| dynamicTaskSize | 存放动态shape任务描述信息所需的内存大小，单位Byte。 |
| fifoTaskSize | 存放模型级别的全局内存大小，单位Byte。<br>若某个模型在推理时，其每一层的输入来自上一层的输出以及前面几轮推理结果拼接而成时，则需使用模型级别的全局内存将该模型所需的输入数据保存下来，供后续推理使用。 |
| reserved | 预留值。 |
