# 量化压缩

<!-- npu="310p,310b" id8 -->
## COMPRESSION\_OPTIMIZE\_CONF

压缩优化功能配置文件路径以及文件名，通过该参数开启配置文件中指定的压缩优化特性，从而提升网络性能。例如：/home/test/compression\_optimize.cfg。此参数实际对应的options参数为`ge.compressionOptimizeConf`。

**配置示例：**

```c++
{ge::ir_option::COMPRESSION_OPTIMIZE_CONF, "/home/test/compression_optimize.cfg"}
```

文件内容配置示例如下：

```text
enable_first_layer_quantization:true
```

- 当前配置文件仅支持配置**enable\_first\_layer\_quantization**特性，用于控制AIPP首层卷积是否进行优化（AIPP会与量化后模型首层卷积CONV2D前的Quant算子进行融合）。

    **开启enable\_first\_layer\_quantization**特性时，只有网络结构中存在AIPP+CONV2D结构，并且模型编译时ENABLE\_SMALL\_CHANNEL参数设置为1时，才有可能获得性能收益。由于量化后的模型存在一定程度上的精度损失，用户根据实际情况决定是否开启该特性。

- 配置文件中冒号前面表示压缩优化特性名称，冒号后面表示是否开启该特性，true表示开启，false表示关闭，默认关闭。

**产品支持情况：**

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：不支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->

<!-- end id8 -->

## SPARSITY

开启全局稀疏特性。此参数实际对应的options参数为`ge.enableSparseMatrixWeight`。

AMCT（昇腾模型压缩工具）4选2结构化稀疏后输出的模型，可能存在weight连续4个Cin维度元素中至少有2个为0的场景，模型转换时通过开启全局稀疏开关，将该场景下的元素筛选成2个，从而节省后续推理的计算量，提高推理性能。

由于硬件约束，该参数不能与ENABLE\_COMPRESS\_WEIGHT、COMPRESS\_WEIGHT\_CONF同时使用。

**参数取值：**

- 1：表示开启4选2结构化稀疏。
- 0：（默认值）不开启稀疏特性。

**配置示例：**

```c++
{ge::ir_option::SPARSITY, "1"}
```

**使用约束：**使用该参数时，请确保模型是稀疏的模型，建议用户使用AMCT（TensorFlow）或AMCT（PyTorch）的组合压缩功能获取，且组合压缩只能是4选2结构化稀疏+量化感知训练模式。

**产品支持情况：**

<!-- npu="950" id9 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id9 -->
<!-- npu="A3" id10 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id10 -->
<!-- npu="910b" id11 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id11 -->
<!-- npu="310b" id12 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id12 -->
<!-- npu="310p" id13 -->
- Atlas 推理系列产品：不支持
<!-- end id13 -->
<!-- npu="910" id14 -->
- Atlas 训练系列产品：不支持
<!-- end id14 -->
- IPV350：不支持
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildInitialize_config_params/quantization_compression_res.md#id1 -->
