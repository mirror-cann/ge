# --op\_select\_implmode

## 功能说明

**该参数功能已停止演进，后续版本会废弃，推荐使用[--op\_precision\_mode](--op_precision_mode.md)参数。**

AI处理器部分内置算子有高精度和高性能实现方式，用户可以通过该参数配置模型编译时算子选择哪种实现方式。

高精度是指在float16输入场景，通过泰勒展开/牛顿迭代等手段进一步提升算子的精度；高性能是指在float16输入的情况下，不影响网络精度前提下的最优性能实现。

## 关联参数

无。

## 参数取值

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

## 推荐配置及收益

**不建议用户使用--op\_select\_implmode参数设置算子的实现模式，该参数仅作为调测使用**，推荐通过[--op\_precision\_mode](--op_precision_mode.md)参数加载ini配置文件方式设置算子精度模式：

- 如果用户对性能有更高要求，则建议优先使用**high\_performance\_for\_all**参数，若经过验证性能满足要求，则建议用户复制一份high\_performance\_for\_all.ini文件，并且重命名为“**网络模型**.ini”文件，跟随网络使用，不同网络模型使用不同的ini文件，后续模型转换时，可以直接使用[--op\_precision\_mode](--op_precision_mode.md)参数加载保存的“**网络模型**.ini”配置文件。
- 如果用户对精度有更高要求，则建议优先使用**high\_precision\_for\_all**参数，若经过验证精度满足要求，则建议用户复制一份high\_precision\_for\_all.ini文件，并且重命名为“**网络模型**.ini”文件，跟随网络使用，不同网络模型使用不同的ini文件，后续模型转换时，可以直接使用[--op\_precision\_mode](--op_precision_mode.md)参数加载保存的_“**网络模型**.ini”_配置文件。
- 如果用户在使用**high\_performance\_for\_all**时，虽然性能得到很大的提升，但是发现精度不满足要求，发现是由于xxx算子使用了高性能模式引起的，则需要复制一份high\_performance\_for\_all.ini文件，重命名为“**网络模型**.ini”文件，并将文件中该xxx算子的实现模式调整为高精度模式，后续模型转换时，直接使用[--op\_precision\_mode](--op_precision_mode.md)参数加载“**网络模型**.ini”配置文件。
- 如果用户在使用**high\_precision\_for\_all**时，虽然精度得到很大的提升，但是发现性能下降较厉害，发现是由于xxx算子使用了高精度模式引起的，则需要复制一份high\_precision\_for\_all.ini文件，重命名为“**网络模型**.ini”文件，并将文件中该xxx算子的实现模式调整为高性能模式，后续模型转换时，直接使用[--op\_precision\_mode](--op_precision_mode.md)参数加载“**网络模型**.ini”配置文件。

high\_\*.ini文件中算子的实现模式以all\_ops\_impl\_mode.ini文件（路径为`${INSTALL_DIR}/opp/built-in/op_impl/ai_core/tbe/impl_mode`）所列出的为准，不在该文件中的实现模式不支持配置。

上述路径中的$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

## 示例

```bash
atc --op_select_implmode=high_precision ...
```

## 使用约束

- 如果有新支持精度模式的算子也选择高性能或者高精度模式，又不想破坏已有网络的精度或性能，则可以通过如下两种方式进行配置：
  - 通过[--optypelist\_for\_implmode](--optypelist_for_implmode.md)参数指定新增的具体算子

    ```text
    --op_select_implmode=high_precision  --optypelist_for_implmode=算子optype
    ```

  - 通过[--op\_precision\_mode](--op_precision_mode.md)参数设置算子的精度模式

    构造算子精度模式配置文件_op\_precision.ini_，并在该文件中设置算子的精度模式，每一行设置一个算子的精度模式，样例如下：

    ```text
    optype1=high_precision
    optype2=high_performance
    ```

    将配置好的_op\_precision.ini_文件上传到ATC工具所在服务器任意目录，例如上传到$HOME/conf，使用示例如下：

    ```text
    --op_precision_mode=$HOME/conf/op_precision.ini
    ```

- [--op\_select\_implmode](--op_select_implmode.md)参数表示设置网络模型中所有算子的高精度或高性能模式，如果算子实现了高精度和高性能，则运行时选择[--op\_select\_implmode](--op_select_implmode.md)参数指定的模式；如果算子只实现了一种，则按照算子实现的方式运行，例如：

    某个算子当前只支持高精度，而[--op\_select\_implmode](--op_select_implmode.md)设置为高性能，则[--op\_select\_implmode](--op_select_implmode.md)参数对于该算子不生效，使用该算子当前实现的高精度方式运行。
