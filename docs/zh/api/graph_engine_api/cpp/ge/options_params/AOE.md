# AOE

## ge.mdl\_bank\_path

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT不支持该参数。
<!-- end id1 -->

加载子图调优后自定义知识库的路径。

该参数需要与ge.bufferOptimize参数配合使用，仅在数据缓存优化开关打开的情况下生效，通过利用高速缓存暂存数据的方式，达到提升性能的目的。

**参数值**：模型调优后自定义知识库路径。

**参数值格式**：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）。

**参数默认值：**$HOME/Ascend/latest/data/aoe/custom/graph/_<soc\_version\>_

**使用约束：**

加载子图调优后自定义知识库路径优先级：ge.mdl\_bank\_path参数加载路径\>**TUNE\_BANK\_PATH**环境变量设置路径\>默认子图调优后自定义知识库路径。

1. 如果模型编译前，通过**TUNE\_BANK\_PATH**环境变量指定了子图调优自定义知识库路径，模型编译时又通过ge.mdl\_bank\_path参数加载了自定义知识库路径，该场景下以ge.mdl\_bank\_path参数加载的路径为准，**TUNE\_BANK\_PATH**环境变量设置的路径不生效。
2. ge.mdl\_bank\_path参数和环境变量指定路径都不生效或无可用自定义知识库，则使用默认自定义知识库路径。
3. 如果上述路径下都无可用的自定义知识库，则会查找子图调优内置知识库，该路径为：$\{INSTALL\_DIR\}/_<arch\>_-linux/data/fusion\_strategy/built-in，_<arch\>_请替换为具体操作系统架构。

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.op\_bank\_path

<!-- npu="950" id2 -->
Ascend 950PR/Ascend 950DT不支持该参数。
<!-- end id2 -->

算子调优后自定义知识库路径。

**参数值格式**：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）。

**参数默认值：**$\{HOME\}/Ascend/latest/data/aoe/custom/op

**使用约束：**

加载算子调优后自定义知识库路径优先级：**TUNE\_BANK\_PATH**环境变量设置路径\>ge.op\_bank\_path参数加载路径\>默认算子调优后自定义知识库路径。

1. 如果模型转换前，通过**TUNE\_BANK\_PATH**环境变量指定了算子调优自定义知识库路径，模型编译时又通过ge.op\_bank\_path参数加载了自定义知识库路径，该场景下以**TUNE\_BANK\_PATH**环境变量设置的路径为准，ge.op\_bank\_path参数加载的路径不生效。
2. ge.op\_bank\_path参数和环境变量指定路径都不生效前提下，使用默认自定义知识库路径。
3. 如果上述路径下都无可用的自定义知识库，则会查找算子调优内置知识库。

**必选/可选**：可选

**生效级别**：全局/session/graph
