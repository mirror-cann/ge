# --mdl\_bank\_path

## 产品支持情况

<!-- npu="950" id7 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id7 -->
<!-- npu="A3" id6 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id6 -->
<!-- npu="910b" id5 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id5 -->
<!-- npu="310b" id4 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id4 -->
<!-- npu="310p" id3 -->
- Atlas 推理系列产品：支持
<!-- end id3 -->
<!-- npu="910" id2 -->
- Atlas 训练系列产品：支持
<!-- end id2 -->
<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--mdl_bank_path_res.md#id1 -->

## 功能说明

加载子图调优后自定义知识库的路径。

子图调优详情请参见[《AOE调优工具》](https://hiascend.com/document/redirect/CannCommunityToolAoe)。

## 关联参数

该参数需要与[--buffer\_optimize](--buffer_optimize.md)参数配合使用，仅在数据缓存优化开关打开的情况下生效，通过利用高速缓存[--buffer\_optimize](--buffer_optimize.md)暂存数据的方式，达到提升性能的目的。

## 参数取值

**参数值：**
子图调优后自定义知识库路径。

**参数值格式：**
支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）。

**参数默认值：**
$HOME/Ascend/latest/data/aoe/custom/graph/_<soc\_version\>_

## 推荐配置及收益

无。

## 示例

例如子图调优后自定义知识库的路径为$HOME/custom\_module\_bank，则使用示例为：

```bash
atc --mdl_bank_path=$HOME/custom_module_path   --buffer_optimize=l2_optimize ...
```

## 依赖约束

加载子图调优后自定义知识库路径优先级：[--mdl\_bank\_path](--mdl_bank_path.md)参数加载路径\>**TUNE\_BANK\_PATH**环境变量设置路径\>默认子图调优后自定义知识库路径。

1. 如果模型转换前，通过**TUNE\_BANK\_PATH**环境变量指定了子图调优自定义知识库路径，模型转换时又通过[--mdl\_bank\_path](--mdl_bank_path.md)参数加载了自定义知识库路径，该场景下以[--mdl\_bank\_path](--mdl_bank_path.md)参数加载的路径为准，**TUNE\_BANK\_PATH**环境变量设置的路径不生效。
2. [--mdl\_bank\_path](--mdl_bank_path.md)参数和环境变量指定路径都不生效或无可用自定义知识库，则使用默认自定义知识库路径。
3. 如果上述路径下都无可用的自定义知识库，则ATC工具会查找子图调优内置知识库，该路径为：$\{INSTALL\_DIR\}/_x86\_64-linux_/data/fusion\_strategy/built-in，*x86\_64-linux*请根据实际操作系统进行替换。
