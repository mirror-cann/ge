# OP\_NO\_REUSE\_MEM

## 功能描述

计算图在昇腾平台编译的过程中默认采用内存复用形式，在问题定位场景中，如果开发者怀疑是内存复用错误导致计算结果异常，可通过此环境变量指定为某算子单独分配内存。

该环境变量可配置为网络节点的名称或者算子的类型（可混合配置），支持一个或多个的配置，配置多个节点时，中间通过英文逗号隔开。

## 配置示例

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

## 使用约束

无

## 产品支持情况

全量芯片支持
