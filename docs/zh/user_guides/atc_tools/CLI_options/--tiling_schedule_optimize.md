# --tiling\_schedule\_optimize

## 产品支持情况

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

## 功能说明

Tiling下沉调度优化开关。

Tiling下沉是在Device侧CPU做Tiling计算。由于NPU中AI Core内部存储无法完全容纳算子输入输出的所有数据，需要每次搬运一部分输入数据进行计算然后搬出，再搬运下一部分输入数据进行计算，该过程称之为Tiling；根据算子的shape等信息来确定数据切分算法相关参数（比如每次搬运的块大小，以及总共循环多少次）的计算程序，称之为Tiling实现。由于Tiling实现中完成的均为标量计算，AI Core并不擅长，故一般在Host侧CPU上执行，但是满足下述条件Tiling实现会下沉到Device侧执行：

1. 模型为静态shape。
2. 模型中的算子支持Tiling下沉，比如FusedInferAttentionScore、IncreFlashAttention等融合算子。
3. 支持Tiling下沉的算子值有依赖，需要满足前一个算子的值有device的执行结果；如果依赖的值是Const，则不需要下沉执行Tiling，编译时会完成Tiling。

## 参数取值

- 0：（默认值）关闭Tiling下沉。
- 1：开启Tiling下沉。

## 推荐配置及收益

无。

## 示例

```bash
atc --tiling_schedule_optimize=1 ...
```
