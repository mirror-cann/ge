# 性能调优

## ENABLE\_SMALL\_CHANNEL

是否开启Small Channel的优化，开启后在Channel<=4的卷积层会有性能收益。建议用户在推理场景下打开此开关。此参数实际对应的options参数为`ge.enableSmallChannel`。

**参数取值：**

- 0：（默认值）关闭。
- 1：开启。

**配置示例：**

```c++
{ge::ir_option::ENABLE_SMALL_CHANNEL, "1"}
```

**产品支持情况：**

全量芯片支持。

<!-- npu="950,A3,910b,910,310p,310b,IPV350" id8 -->
## TILING\_SCHEDULE\_OPTIMIZE

Tiling下沉调度优化开关。此参数实际对应的options参数为`ge.tiling_schedule_optimize`。

由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，该过程称之为Tiling；根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，故一般在Host侧CPU上执行，但是满足下述条件Tiling实现会下沉到Device侧执行：

1. 模型为静态shape。
2. 模型中的算子支持Tiling下沉，比如FusedInferAttentionScore、IncreFlashAttention等融合算子。
3. 支持Tiling下沉的算子值有依赖，需要满足前一个算子的值有device的执行结果；如果依赖的值是Const，则不需要下沉执行Tiling，编译时会完成Tiling。

**参数取值：**

- 0：关闭Tiling下沉，默认为0。
- 1：开启Tiling下沉。

**配置示例：**

```c++
{ge::ir_option::TILING_SCHEDULE_OPTIMIZE, "1"}
```

**产品支持情况：**

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
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
