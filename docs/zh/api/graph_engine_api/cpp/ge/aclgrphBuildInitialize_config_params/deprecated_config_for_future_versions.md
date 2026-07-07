# 后续版本废弃配置

## OP\_SELECT\_IMPL\_MODE

**该参数功能已停止演进，后续版本会废弃。**

AI处理器部分内置算子有高精度和高性能实现方式，用户可以通过该参数配置模型编译时算子选择哪种实现方式。

高精度是指在float16输入场景，通过泰勒展开/牛顿迭代等手段进一步提升算子的精度；高性能是指在float16输入的情况下，不影响网络精度前提下的最优性能实现。

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

上述实现模式，根据算子的dtype进行区分。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**参数默认值：**high\_performance

**配置示例：**

```c++
{ge::ir_option::OP_SELECT_IMPL_MODE, "high_performance"}
```

## OPTYPELIST\_FOR\_IMPLMODE

**该参数功能已停止演进，后续版本会废弃，请勿使用。**

设置optype列表中算子的实现模式。

**参数值约束：**

- 列表中的算子使用OP\_SELECT\_IMPL\_MODE参数指定的模式，当前仅支持指定为high\_precision、high\_performance两种模式，多个算子使用英文逗号进行分隔。
- 该参数需要与OP\_SELECT\_IMPL\_MODE参数配合使用，且仅对指定的算子生效，不指定的算子按照默认实现方式选择。例如：OP\_SELECT\_IMPL\_MODE配置为high\_precision；OPTYPELIST\_FOR\_IMPLMODE配置为Pooling、SoftmaxV2。表示对Pooling、SoftmaxV2算子使用统一的高精度模式，未指定算子使用算子的默认实现方式。

**配置示例：**

```c++
{ge::ir_option::OPTYPELIST_FOR_IMPLMODE, "Pooling,SoftmaxV2"}
```
