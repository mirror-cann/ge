# --enable\_small\_channel

## 产品支持情况

全量芯片支持。

## 功能说明

是否开启small channel的优化，开启后在channel<=4的卷积层会有性能收益。建议用户在推理场景下打开此开关。

## 关联参数

- 配置该参数后，建议与[--insert\_op\_conf](--insert_op_conf.md)参数（AIPP功能）配合使用，可以获得更优的性能。在配合使用时，由于软件约束，只能和静态AIPP配合使用，不能和动态AIPP配合使用。

## 参数取值

**参数值：**

- 0：（默认值）关闭，模型推理时关闭small channel优化。
- 1：开启，模型推理时开启small channel优化。

**参数值约束：**
如果模型Input的channel<=4，建议开启该参数，并配合静态AIPP（[--insert\_op\_conf](--insert_op_conf.md)）使用，可获得更优的性能；如果开启之后出现性能下降，建议进行Tiling调优。

## 推荐配置及收益

无。

## 示例

```bash
atc --enable_small_channel=1 ...
```

## 使用约束

该参数开启后，建议与AIPP功能[--insert\_op\_conf](--insert_op_conf.md)同时使用，否则可能没有收益。
