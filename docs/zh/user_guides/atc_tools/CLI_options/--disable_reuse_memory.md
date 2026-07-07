# --disable\_reuse\_memory

## 产品支持情况

全量芯片支持

## 功能说明

内存复用开关

内存复用是指按照生命周期和内存大小，把不冲突的内存重复使用，来降低网络内存占用。

## 关联参数

无。

## 参数取值

- 0：（默认值）开启内存复用。
- 1：关闭内存复用。如果网络模型较大，关闭内存复用开关，会造成后续推理时Device侧内存不复用，从而导致内存不足。

## 推荐配置及收益

无。

## 示例

```bash
atc --disable_reuse_memory=0 ...
```

## 依赖约束

在内存复用场景下（默认开启内存复用），支持基于指定算子（节点名称/算子类型）单独分配内存。通过OP\_NO\_REUSE\_MEM环境变量指定要单独分配的一个或多个节点，支持混合配置。配置多个节点时，中间通过英文逗号\(“,”\)隔开。详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

- 基于节点名称配置

    节点名称需要配置为转换为CANN平台网络后的节点名称，节点名称可以通过设置DUMP\_GE\_GRAPH环境变量，然后在导出的ge\_onnx\_xxx\_Build.pbtxt最终图中查看“name”字段获取。

    ```bash
    export OP_NO_REUSE_MEM=gradients/logits/semantic/kernel/Regularizer/l2_regularizer_grad/Mul_1,resnet_v1_50/conv1_1/BatchNorm/AssignMovingAvg2
    ```

- 基于算子类型配置

    ```bash
    export OP_NO_REUSE_MEM=FusedMulAddN,BatchNorm
    ```

- 混合配置

    ```bash
    export OP_NO_REUSE_MEM=FusedMulAddN,resnet_v1_50/conv1_1/BatchNorm/AssignMovingAvg
    ```
