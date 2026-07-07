# --allow\_hf32

## 产品支持情况

<!-- npu="950" id7 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id7 -->
<!-- npu="A3" id6 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id6 -->
<!-- npu="910b" id5 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id5 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id4 -->
<!-- npu="310p" id3 -->
- Atlas 推理系列产品：不支持
<!-- end id3 -->
<!-- npu="910" id2 -->
- Atlas 训练系列产品：不支持
<!-- end id2 -->

## 功能说明

**该参数预留，当前版本暂不支持**。

是否启用HF32自动代替float32数据类型的功能，当前版本该参数仅针对Conv类算子与Matmul类算子生效。

HF32是昇腾推出的专门用于算子内部计算的单精度浮点类型，与其他常用数据类型的比较如下图所示。可见，HF32与float32支持相同的数值范围，但尾数位精度（11位）却接近FP16（10位）。通过降低精度让HF32单精度数据类型代替原有的float32单精度数据类型，可大大降低数据所占空间大小，实现性能的提升。

**图 1**  HF32与其他数据类型比较
![图示](../figures/hf32_data_type_comparison.png "HF32与其他数据类型比较")

## 关联参数

- 针对同一个算子，如果通过[--op\_precision\_mode](--op_precision_mode.md)参数配置了enable\_hi\_float\_32\_execution或enable\_float\_32\_execution，该场景下不建议再与[--allow\_hf32](--allow_hf32.md)参数同时使用，若同时使用，则优先级如下：

    op\_precision\_mode\(ByNodeName，按节点名称设置精度模式\) \> allow\_hf32 \> op\_precision\_mode\(ByOpType，按算子类型设置精度模式\)

- 由于--allow\_hf32是使用HF32自动代替float32，要想该参数生效，必须保证涉及算子的输入或者输出类型为float32。由于[--precision\_mode\_v2](--precision_mode_v2.md)参数默认值为fp16，原始网络模型中算子类型为float32时会被强制转为float16类型，该场景下使用--allow\_hf32参数不生效，建议修改[--precision\_mode\_v2](--precision_mode_v2.md)参数值为**origin**（[--precision\_mode](--precision_mode.md)参数默认值为force\_fp16，建议修改为**must\_keep\_origin\_dtype**或者**force\_fp32**）。

## 参数取值

**参数值：**

- true：针对Conv类算子与Matmul类算子，开启FP32数据类型自动转换为HF32数据类型的功能。

    具体涉及哪些算子，请参见CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/allow\_hf32\_matmul\_**t**\_conv\_**t**.ini，该文件不支持用户修改。

- false：针对Conv类算子与Matmul类算子，关闭FP32数据类型自动转换为HF32数据类型的功能。

    具体涉及哪些算子，请参见CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/allow\_hf32\_matmul\_**f**\_conv\_**f**.ini，该文件不支持用户修改。

**参数默认值：**
针对Conv类算子，默认开启FP32到HF32的转换；针对Matmul类算子，默认关闭该转换功能。

## 推荐配置及收益

无。

## 示例

```bash
atc --allow_hf32=true ...
```

若在模型转换日志中能搜索到“ge.exec.allow\_hf32”则说明参数设置生效。

## 依赖约束

无。
