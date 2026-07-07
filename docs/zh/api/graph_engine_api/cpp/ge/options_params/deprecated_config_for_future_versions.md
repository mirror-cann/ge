# 后续版本废弃配置

## ge.exec.dataInputsShapeRange

**该参数已废弃，请勿使用。**

动态输入的shape范围。例如全图有2个data输入，配置示例为：

```c++
std::map<ge::AscendString, ge::AscendString> ge_options = {{"ge.exec.deviceId", "0"},
      {"ge.graphRunMode", "1"},
      {"ge.exec.dynamicGraphExecuteMode", "dynamic_execute"},
      {"ge.exec.dataInputsShapeRange", "[128 ,3~5, 2~128, -1],[ 128 ,3~5, 2~128, -1]"}};
```

- 设置格式为："\[n1,c1,h1,w1\],\[n2,c2,h2,w2\]"，例如："\[8\~20,3,5,-1\],\[5,3\~9,10,-1\]"。可以不指定节点名，默认第一个中括号为第一个输入节点，节点中间使用英文逗号分隔。按照index设置INPUT\_SHAPE\_RANGE时，data节点需要设置属性index，说明是第几个输入，index从0开始。
- 动态维度有shape范围的用波浪号“\~”表示，固定维度用固定数字表示，无限定范围的用-1表示。
- 对于标量输入，也需要填入shape范围，表示方法为：\[\]。
- 对于多输入场景，例如有三个输入时，如果只有第二个第三个输入具有shape范围，第一个输入为固定输入时，仍需要将固定输入shape填入options字段内，例如：

    \{"ge.exec.dataInputsShapeRange", "\[3,3,4,10\], \[-1,3,2\~1000,-1\],\[-1,-1,-1,-1\]"\}\};

> [!NOTE]说明
>
>- 若没有指定节点名，则节点默认按照index顺序存储，存储示例如下：
> xxx\_0，xxx\_1，xxx\_2，……
> 其中下划线后为节点在网络脚本中的定义顺序索引，节点会按照此索引的字母顺序进行排布，所以当节点的个数大于10时，则排序为“xxx\_0 -\> xxx\_10 -\> xxx\_2 -\> xxx\_3”，网络脚本中定义索引为10的节点排在了索引为2的节点前面，导致定义的shape range与实际输入的节点不匹配。
> 为避免此问题，当节点的输入个数大于10时，建议在网络脚本中指定节点的名称，则节点会以指定的名称进行命名，实现shape range与节点名称的关联。
>- 如果该参数与ge.dynamicDims同时配置，示例如下：
>
>   ```c++
>   std::map<ge::AscendString, ge::AscendString> ge_options =
>       {{"ge.inputShape", "data:1,1,40,-1;label:1,-1;mask:-1,-1" },
>        {"ge.dynamicDims", "20,20,1,1;40,40,2,2;80,60,4,4"},
>           xxx
>        {"ge.exec.dataInputsShapeRange", "[128, 3~5, 2~128, -1],[ 128 ,3~5, 2~128, -1]"}};
>       ```
>
> ge.dynamicDims参数功能（动态分档）优先级高于ge.exec.dataInputsShapeRange参数功能（动态shape范围）。

**必选/可选**：可选

**生效级别**：graph

## ge.exec.dynamicGraphExecuteMode

**该参数已废弃，请勿使用。**

对于动态输入场景，需要通过该参数设置执行模式，取值为：dynamic\_execute。

**必选/可选**：可选

**生效级别**：graph

## ge.graphMemoryMaxSize

**该参数后续版本废弃，请勿使用。**

网络静态内存和最大动态内存，可根据网络大小指定。单位：Byte，取值范围：\[0, 256\*1024\*1024\*1024\]或\[0, 274877906944\]。当前受芯片硬件限制，ge.graphMemoryMaxSize和ge.variableMemoryMaxSize总和最大支持31G。如果不设置，默认为26GB。

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.opSelectImplmode

**该参数功能已经不演进，后续版本会废弃，推荐使用ge.exec.op\_precision\_mode参数。**

AI处理器部分内置算子有高精度和高性能实现方式，用户可以通过该参数配置模型编译时选择哪种算子。

高精度是指在float16输入场景，通过泰勒展开/牛顿迭代等手段进一步提升算子的精度；高性能是指在float16输入的情况下，不影响网络精度前提的最优性能实现。

**参数取值：**

- **high\_precision**：表示算子采用高精度实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/impl_mode/high_precision.ini`。

    为保持兼容，该参数仅对high\_precision.ini文件中算子列表生效，通过该列表可以控制算子生效的范围并保证之前版本的网络模型不受影响。

- **high\_performance**：（默认值）表示算子采用高性能实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/impl_mode/high_performance.ini`。

    为保持兼容，该参数仅对high\_performance.ini文件中算子列表生效，通过该列表可以控制算子生效的范围并保证之前版本的网络模型不受影响。

- **high\_precision\_for\_all**：表示算子采用高精度实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/impl_mode/high_precision_for_all.ini`，该文件中列表后续可能会跟随版本更新。

    **该实现模式不保证兼容**，如果后续新的软件包中有算子新增了实现模式（即配置文件中新增了某个算子的实现模式），之前版本使用high\_precision\_for\_all的网络模型，在新版本上性能可能会下降。

- **high\_performance\_for\_all**：表示算子采用高性能实现模式。

    该选项采用系统内置的配置文件设置算子实现模式，内置配置文件路径为`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/impl_mode/high_performance_for_all.ini`，该文件中列表后续可能会跟随版本更新。

    **该实现模式不保证兼容**，如果后续新的软件包中有算子新增了实现模式（即配置文件中新增了某个算子的实现模式），之前版本使用high\_performance\_for\_all的网络模型，在新版本上精度可能会下降。

上述实现模式，根据算子的dtype进行区分。其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**配置示例：**

```c++
{"ge.opSelectImplmode", "high_performance"};
```

**必选/可选**：可选

**生效级别**：全局

## ge.optypelistForImplmode

列举算子optype的列表，该列表中的算子使用ge.opSelectImplmode参数指定的模式。

**参数值约束：**

- 列表中的算子使用ge.opSelectImplmode参数指定的模式，当前仅支持指定为high\_precision、high\_performance两种模式，多个算子使用英文逗号进行分隔。
- 该参数需要与ge.opSelectImplmode参数配合使用，且仅对指定的算子生效，不指定的算子按照默认实现方式选择。例如：ge.opSelectImplmode配置为high\_precision；ge.optypelistForImplmode配置为Pooling、SoftmaxV2。表示对Pooling、SoftmaxV2算子使用统一的高精度模式，未指定算子使用算子的默认实现方式。

**必选/可选**：可选

**生效级别**：全局

## ge.shape\_generalized\_build\_mode

**该参数在后续版本废弃、请勿使用。**

**必选/可选**：可选

**生效级别**：graph

## ge.variableMemoryMaxSize

**该参数后续版本废弃，请勿使用。**

变量内存，可根据网络大小指定。单位：Byte，取值范围：\[0, 256\*1024\*1024\*1024\]或\[0, 274877906944\]。当前受芯片硬件限制，ge.graphMemoryMaxSize和ge.variableMemoryMaxSize总和最大支持31G。如果不设置，默认为5GB。

**必选/可选**：可选

**生效级别**：全局/session/graph
