# 试验参数

<!-- npu="950,A3,910b" id1 -->
## ALLOW\_HF32

**该参数预留，当前版本暂不支持**。

是否启用HF32自动代替float32数据类型的功能，当前版本该参数仅针对Conv类算子与Matmul类算子生效。此参数实际对应的options参数为`ge.exec.allow_hf32`。

HF32是昇腾推出的专门用于算子内部计算的单精度浮点类型，与其他常用数据类型的比较如下图所示。可见，HF32与float32支持相同的数值范围，但尾数位精度（11位）却接近FP16（10位）。通过降低精度让HF32单精度数据类型代替原有的float32单精度数据类型，可大大降低数据所占空间大小，实现性能的提升。

![图示](../../../figures/hf32.png)

**参数取值：**

- true：针对Conv类算子与Matmul类算子，开启FP32数据类型自动转换为HF32数据类型的功能。

    具体涉及哪些算子，请参见CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/allow\_hf32\_matmul\_**t**\_conv\_**t**.ini，该文件不支持用户修改。

- false：针对Conv类算子与Matmul类算子，关闭FP32数据类型自动转换为HF32数据类型的功能。

    具体涉及哪些算子，请参见CANN软件安装后文件存储路径的opp/built-in/op\_impl/ai\_core/tbe/impl\_mode/allow\_hf32\_matmul\_**f**\_conv\_**f**.ini，该文件不支持用户修改。

**参数默认值：**针对Conv类算子，默认开启FP32到HF32的转换；针对Matmul类算子，默认关闭该转换功能。

**使用约束**：

- 针对同一个算子，如果通过OP\_PRECISION\_MODE参数配置了enable\_hi\_float\_32\_execution或enable\_float\_32\_execution，该场景下不建议再与ALLOW\_HF32参数同时使用，若同时使用，则优先级如下：

    OP\_PRECISION\_MODE\(ByNodeName，按节点名称设置精度模式\) \> ALLOW\_HF32 \> OP\_PRECISION\_MODE\(ByOpType，按算子类型设置精度模式\)。

- 由于ALLOW\_HF32是使用HF32自动代替float32，要想该参数生效，必须保证被涉及算子的输入或者输出类型为float32。由于PRECISION\_MODE\_V2参数默认值为fp16，原始网络模型中算子类型为float32时会被强制转为float16类型，该场景下使用ALLOW\_HF32参数不生效，建议修改PRECISION\_MODE\_V2参数值为**origin**（PRECISION\_MODE参数默认值为force\_fp16，建议修改为**must\_keep\_origin\_dtype**或者**force\_fp32**）。

**配置示例：**

```c++
{ge::ir_option::ALLOW_HF32, "true"}
```

**产品支持情况：**

<!-- npu="950" id2 -->
- Ascend 950PR/Ascend 950DT：支持
<!-- end id2 -->
<!-- npu="A3" id3 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id3 -->
<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id4 -->
<!-- npu="310b" id5 -->
- Atlas 200I/500 A2 推理产品：不支持
<!-- end id5 -->
<!-- npu="310p" id6 -->
- Atlas 推理系列产品：不支持
<!-- end id6 -->
<!-- npu="910" id7 -->
- Atlas 训练系列产品：不支持
<!-- end id7 -->
<!-- npu="IPV350" id8 -->
- IPV350：不支持
<!-- end id8 -->

<!-- end id1 -->

## OO\_CONSTANT\_FOLDING

**调试功能扩展参数，暂不支持应用于生产环境，后续版本会作为正式功能更新发布。**

是否开启常量折叠优化。此参数实际对应的options参数为`ge.oo.constantFolding`。

常量折叠是将计算图中可以预先确定输出值的节点替换成常量，并对计算图进行一些结构简化的操作。

**参数取值：**

- true：（默认值）开启。
- false：关闭。

**配置示例：**

```c++
{ge::ir_option::OO_CONSTANT_FOLDING, "true"}
```

**产品支持情况：**

全量芯片支持。

## OO\_DEAD\_CODE\_ELIMINATION

**调试功能扩展参数，暂不支持应用于生产环境，后续版本会作为正式功能更新发布。**

是否开启死边消除优化。此参数实际对应的options参数为`ge.oo.deadCodeElimination`。

死边消除：switch死边消除，switch的pred输入（1号输入）为const节点时，根据const的值消除其中一条分支：const为true时，消除false分支；const为false时，消除true分支。

**参数取值：**

- true：（默认值）开启。
- false：关闭。

**配置示例：**

```c++
{ge::ir_option::OO_DEAD_CODE_ELIMINATION, "true"}
```

**产品支持情况：**

全量芯片支持。

## OO\_LEVEL

**调试功能扩展参数，暂不支持应用于生产环境，后续版本会作为正式功能更新发布。**

图编译多级优化选项，包括子图优化、整图优化、静态Shape模型下沉等。此参数实际对应的options参数为`ge.oo.level`。

静态Shape模型下沉：静态Shape模型在编译时即可确定所有算子的输入输出Shape，完成模型级内存编排、算子的Tiling计算等Host侧计算，在模型加载时整体下发到Device流上，但不立即执行，通过下发模型执行Task触发模型中所有Task的执行。

**参数取值：**

- O1：会关闭所有图融合和UB融合Pass，只做促成静态下沉的相关优化，如InferShape（进行输出Tensor的shape推导）、常量折叠、死边消除等。

  <!-- npu="950" id9 -->
  Ascend 950PR/Ascend 950DT不支持UB融合，只关闭图融合Pass。
  <!-- end id9 -->

- O3：（默认值）开启所有优化。

**使用约束：**

取值为O1时，会关闭所有图融合和UB融合PASS，只开启静态下沉的相关PASS，但是如下路径文件中的图融合PASS，由于关闭后会有功能问题，会默认开启：

`${INSTALL_DIR}/_<arch>_-linux/lib64/plugin/opskernel/fusion_pass/config/fusion_config.json`文件中"ExceptionalPassOfO1Level"字段下的所有图融合PASS。

其中$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。<arch\>表示具体操作系统架构。

**配置示例：**

```c++
{ge::ir_option::OO_LEVEL, "O3"}
```

**产品支持情况：**

全量芯片支持。

## TUNE\_DEVICE\_IDS

**当前版本暂不支持**。
