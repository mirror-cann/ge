# AOE

## MDL\_BANK\_PATH

加载子图调优后自定义知识库的路径。此参数实际对应的options参数为`ge.mdl_bank_path`。

该参数需要与[精度比对](../aclgrphBuildInitialize_config_params/precision_comparison.md)中的BUFFER\_OPTIMIZE参数配合使用，仅在数据缓存优化开关打开的情况下生效，通过利用高速缓存暂存数据的方式，达到提升性能的目的。

**参数值**：模型调优后自定义知识库路径。

**参数值格式**：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）。

**参数默认值**：$HOME/Ascend/latest/data/aoe/custom/graph/_<soc\_version\>_

**配置示例：**

```c++
{ge::ir_option::MDL_BANK_PATH, "$HOME/custom_module_path"}
```

**使用约束：**

加载子图调优后自定义知识库路径优先级：MDL\_BANK\_PATH参数加载路径\>**TUNE\_BANK\_PATH**环境变量设置路径\>默认子图调优后自定义知识库路径。

1. 如果模型编译前，通过**TUNE\_BANK\_PATH**环境变量指定了子图调优自定义知识库路径，模型编译时又通过MDL\_BANK\_PATH参数加载了自定义知识库路径，该场景下以MDL\_BANK\_PATH参数加载的路径为准，**TUNE\_BANK\_PATH**环境变量设置的路径不生效。
2. MDL\_BANK\_PATH参数和环境变量指定路径都不生效或无可用自定义知识库，则使用默认自定义知识库路径。
3. 如果上述路径下都无可用的自定义知识库，则会查找子图调优内置知识库，该路径为：$\{INSTALL\_DIR\}/_<arch\>_-linux/data/fusion\_strategy/built-in，_<arch\>_表示具体操作系统架构。

**产品支持情况：**

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id1 -->
<!-- npu="A3" id2 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->
<!-- npu="910b" id3 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id5 -->
- Atlas 推理系列产品：支持
<!-- end id5 -->
<!-- npu="910" id6 -->
- Atlas 训练系列产品：支持
<!-- end id6 -->
<!-- npu="IPV350" id7 -->
- IPV350：不支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/AOE_res.md#id1 -->

## OP\_BANK\_PATH

算子调优后自定义知识库路径。此参数实际对应的options参数为`ge.op_bank_path`。

**参数值格式**：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）。

**参数默认值**：$\{HOME\}/Ascend/latest/data/aoe/custom/op

**配置示例：**

```c++
{ge::ir_option::OP_BANK_PATH, "$HOME/custom_tune_path"}
```

**使用约束：**

加载算子调优后自定义知识库路径优先级：**TUNE\_BANK\_PATH**环境变量设置路径\>OP\_BANK\_PATH参数加载路径\>默认算子调优后自定义知识库路径。

1. 如果模型转换前，通过**TUNE\_BANK\_PATH**环境变量指定了算子调优自定义知识库路径，模型编译时又通过OP\_BANK\_PATH参数加载了自定义知识库路径，该场景下以**TUNE\_BANK\_PATH**环境变量设置的路径为准，OP\_BANK\_PATH参数加载的路径不生效。
2. OP\_BANK\_PATH参数和环境变量指定路径都不生效前提下，使用默认自定义知识库路径。
3. 如果上述路径下都无可用的自定义知识库，则会查找算子调优内置知识库。

**产品支持情况：**

<!-- npu="950" id8 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id8 -->
<!-- npu="A3" id9 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id9 -->
<!-- npu="910b" id10 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id10 -->
<!-- npu="310b" id11 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id11 -->
<!-- npu="310p" id12 -->
- Atlas 推理系列产品：支持
<!-- end id12 -->
<!-- npu="910" id13 -->
- Atlas 训练系列产品：支持
<!-- end id13 -->
<!-- npu="IPV350" id14 -->
- IPV350：不支持
<!-- end id14 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphBuildModel_config_params/AOE_res.md#id2 -->
