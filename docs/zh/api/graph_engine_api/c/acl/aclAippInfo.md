# aclAippInfo

```c
typedef struct aclAippInfo {
    aclAippInputFormat inputFormat;
    int32_t srcImageSizeW;
    int32_t srcImageSizeH;
    int8_t cropSwitch;
    int32_t loadStartPosW;
    int32_t loadStartPosH;
    int32_t cropSizeW;
    int32_t cropSizeH;
    int8_t resizeSwitch;
    int32_t resizeOutputW;
    int32_t resizeOutputH;
    int8_t paddingSwitch;
    int32_t leftPaddingSize;
    int32_t rightPaddingSize;
    int32_t topPaddingSize;
    int32_t bottomPaddingSize;
    int8_t cscSwitch;
    int8_t rbuvSwapSwitch;
    int8_t axSwapSwitch;
    int8_t singleLineMode;
    int32_t matrixR0C0;
    int32_t matrixR0C1;
    int32_t matrixR0C2;
    int32_t matrixR1C0;
    int32_t matrixR1C1;
    int32_t matrixR1C2;
    int32_t matrixR2C0;
    int32_t matrixR2C1;
    int32_t matrixR2C2;
    int32_t outputBias0;
    int32_t outputBias1;
    int32_t outputBias2;
    int32_t inputBias0;
    int32_t inputBias1;
    int32_t inputBias2;
    int32_t meanChn0;
    int32_t meanChn1;
    int32_t meanChn2;
    int32_t meanChn3;
    float minChn0;
    float minChn1;
    float minChn2;
    float minChn3;
    float varReciChn0;
    float varReciChn1;
    float varReciChn2;
    float varReciChn3;
    aclFormat srcFormat;
    aclDataType srcDatatype;
    size_t srcDimNum;
    size_t shapeCount;
    aclAippDims outDims[ACL_MAX_SHAPE_COUNT];
    aclAippExtendInfo *aippExtend;
} aclAippInfo;
```

| 成员名称 | 说明 |
| --- | --- |
| inputFormat | 输入图片格式。类型定义请参见[aclAippInputFormat](aclAippInputFormat.md)。 |
| srcImageSizeW | 原始图片的宽。 |
| srcImageSizeH | 原始图片的高。 |
| cropSwitch | 抠图开关。 |
| loadStartPosW | 抠图时，坐标点起始位置在图中横向的坐标。 |
| loadStartPosH | 抠图时，坐标点起始位置在图中纵向的坐标。 |
| cropSizeW | 抠图区域的宽度。 |
| cropSizeH | 抠图区域的高度。 |
| resizeSwitch | 缩放开关。 |
| resizeOutputW | 缩放后图片的宽。 |
| resizeOutputH | 缩放后图片的高。 |
| paddingSwitch | 是否对图片执行补边操作。 |
| leftPaddingSize | 在图片左方填充的值。 |
| rightPaddingSize | 在图片右方填充的值。 |
| topPaddingSize | 在图片上方填充的值。 |
| bottomPaddingSize | 在图片下方填充的值。 |
| cscSwitch | 色域转换开关。 |
| rbuvSwapSwitch | 表示是否交换R通道与B通道、或者是否交换U通道与V通道的开关。 |
| axSwapSwitch | 表示RGBA->ARGB或者YUVA->AYUV的交换开关。 |
| singleLineMode | 单行处理模式开关，只处理抠图后的第一行。 |
| matrixR0C0 | 色域转换矩阵参数。 |
| matrixR0C1 | 色域转换矩阵参数。 |
| matrixR0C2 | 色域转换矩阵参数。 |
| matrixR1C0 | 色域转换矩阵参数。 |
| matrixR1C1 | 色域转换矩阵参数。 |
| matrixR1C2 | 色域转换矩阵参数。 |
| matrixR2C0 | 色域转换矩阵参数。 |
| matrixR2C1 | 色域转换矩阵参数。 |
| matrixR2C2 | 色域转换矩阵参数。 |
| outputBias0 | RGB转YUV时的输出偏移。 |
| outputBias1 | RGB转YUV时的输出偏移。 |
| outputBias2 | RGB转YUV时的输出偏移。 |
| inputBias0 | YUV转RGB时的输入偏移。 |
| inputBias1 | YUV转RGB时的输入偏移。 |
| inputBias2 | YUV转RGB时的输入偏移。 |
| meanChn0 | 通道0的均值。 |
| meanChn1 | 通道1的均值。 |
| meanChn2 | 通道2的均值。 |
| meanChn3 | 通道3的均值。 |
| varReciChn0 | 通道0的方差的倒数。 |
| varReciChn1 | 通道1的方差的倒数。 |
| varReciChn2 | 通道2的方差的倒数。 |
| varReciChn3 | 通道3的方差的倒数。 |
| srcFormat | 模型转换前，原始模型的输入format。<br>类型定义请参见aclFormat。 |
| srcDatatype | 模型转换前，原始模型的输入datatype。<br>类型定义请参见aclDataType。 |
| srcDimNum | 模型转换前，原始模型输入dims。 |
| shapeCount | 动态Shape（动态Batch或动态分辨率）场景下的档位数。 |
| outDims | 在多shape模型下，各个分档对应的aipp输出dims信息，需要在模型编译阶段按照shape推导出来，保存在模型中，输出的Format，datatype、输出dims等不随shape变化而变化。<br>类型定义请参见[aclAippDims](aclAippDims.md)。 |
| aippExtend | 预留参数，当前版本用户必须传入空指针。 |
