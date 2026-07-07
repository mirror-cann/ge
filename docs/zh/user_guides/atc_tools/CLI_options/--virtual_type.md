# --virtual\_type

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id5 -->
全量芯片支持
<!-- end id5 -->

<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--virtual_type_res.md#id1 -->

## 功能说明

是否支持离线模型在昇腾虚拟化实例特性生成的虚拟设备上运行。

当前芯片算力比较大，云端用户或者小企业完全不需要使用这么大算力，昇腾虚拟化实例特性支持对芯片的算力进行切分，可满足用户按照自己的业务按需申请算力的需求。

昇腾虚拟化实例主要是指NPU资源虚拟化的场景，其主要目的是为了通过相互隔离的vNPU虚拟实例提升物理NPU资源利用率。虚拟设备是按照指定算力在芯片上申请的虚拟加速资源。

## 参数取值

- 0：（默认值）离线模型不在昇腾虚拟化实例特性生成的虚拟设备上运行。
- 1：离线模型在不同算力的虚拟设备上运行。

## 推荐配置及收益

无。

## 示例

```bash
atc --virtual_type=1 ...
```

## 使用约束

- 使用该参数时，请确保运行环境已经搭建好昇腾虚拟化实例特性环境。
- 针对MindSpore框架：
  - **ReduceMean**算子不支持使用--virtual\_type参数。
  <!-- npu="910,310p" id2 -->
  - **ReverseV2**算子仅在Atlas 推理系列产品、Atlas 训练系列产品支持使用--virtual\_type参数。
  <!-- end id2 -->

- 若使用[--virtual\_type](--virtual_type.md)=1进行模型转换，则转换后离线模型参与计算的逻辑AI Core核数可能比实际aicore\_num核数大，为aicore\_num支持配置范围的最小公倍数：

    例如aicore\_num支持配置范围为\{1,2,4,8\}，则使用[--virtual\_type](--virtual_type.md)=1参数转换后的离线模型，NPU运行核数为8。

<!-- npu="950,A3,910b,910,310p,310b" id3 -->
- [--virtual\_type](--virtual_type.md)=1时，使用ATC工具转换后的模型，如果包括如下算子，会默认使用单核，该场景下，将会导致转换后的模型推理性能下降：

  - DynamicRNN
  - PadV2D
  - SquareSumV2
  - DynamicRNNV2
  - DynamicRNNV3
  - DynamicGRUV

    <!-- npu="950" id4 -->
    Ascend 950PR/Ascend 950DT不支持该约束。
    <!-- end id4 -->

<!-- end id3 -->
