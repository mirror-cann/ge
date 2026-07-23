# 准备环境

## 获取ATC工具

进行模型转换前，请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)完成环境搭建，并确保已安装CANN Toolkit开发套件包和ops算子包（**针对8.5.0及之后版本，**必须安装与目标AI处理器相匹配的ops算子包，否则会导致编译失败）。以root用户的默认安装路径为例，ATC工具安装在“/usr/local/Ascend/cann/bin”目录中。

支持在同一开发环境中部署多个芯片的算子包，实现多芯片场景下的模型转换，可通过在不同路径下分别安装CANN Toolkit开发套件及对应芯片（如芯片A、芯片B）的算子包，例如：

- 路径1：安装CANN Toolkit + 芯片A的ops算子包
- 路径2：安装CANN Toolkit + 芯片B的ops算子包

然后执行不同路径下的环境变量，实现不同芯片间算子包的切换，满足多芯片环境下的开发与部署需求。

## 设置环境变量

> [!NOTE]说明
>
>- 使用export方式设置环境变量后，环境变量只在当前窗口有效，用户可以按需将以上命令写入环境变量配置文件（如.bashrc文件）。
>- 使用ATC工具进行模型转换的过程中，会自动将ATC工具所在位置“../python/site-packages”目录下算子编译依赖的Python库写入PYTHONPATH环境变量。
> 若算子实现时用户引入了上述依赖外的其他Python依赖，请自行添加PYTHONPATH的环境变量，配置引入的Python依赖所在路径，如下所示：
> export PYTHONPATH=_xxxx_:$PYTHONPATH

1. **必选环境变量**

    - **设置公共环境变量**

        安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行如下环境变量：

        ```bash
        source /usr/local/Ascend/cann/set_env.sh
        #若CANN Toolkit开发套件包和ops算子包在非昇腾设备上安装，则如下环境变量必须执行，用于设置动态链接库所在路径，否则无需执行
        export LD_LIBRARY_PATH=/usr/local/Ascend/cann/<arch>-linux/devlib:$LD_LIBRARY_PATH
        ```

        其中，/usr/local/Ascend/为root用户的默认安装路径，请根据实际情况进行替换，_<arch\>_请替换为操作系统具体架构。

    - **设置Python相关环境变量**

        模型编译依赖Python，以Python3.7.5为例，请以CANN软件包运行用户执行如下命令设置Python3.7.5相关环境变量。

        ```bash
        #如果用户环境存在多个python3版本，则指定使用python3.7.5版本
        export PATH=/usr/local/python3.7.5/bin:$PATH
        #设置python3.7.5库文件路径
        export LD_LIBRARY_PATH=/usr/local/python3.7.5/lib:$LD_LIBRARY_PATH
        ```

    <!-- @ref: ge/res/docs/zh/user_guides/atc_tools/overview/environment_setup_res.md#id1 -->

    上述环境变量只在当前窗口生效，用户可以将上述命令写入\~/.bashrc文件，使其永久生效，方法如下：

    1. 以安装用户在任意目录下执行**vi \~/.bashrc**，在该文件最后添加上述内容。
    2. 保存文件中，执行**source \~/.bashrc**使环境变量生效。

2. **可选环境变量**
    1. 日志落盘、打屏与重定向。
        <!-- npu="950,A3,910b,910,310p,310b" id1 -->
        - **日志落盘**：

            atc命令执行过程中，日志默认落盘到如下路径：

            - $HOME/ascend/log/**debug**/plog/plog-_pid_\_\*.log：调试日志。

                调试日志场景，由于[--log](../CLI_options/--log.md)默认值为null，即不输出日志，若上述路径存在日志信息，则为atc进程之外的其他日志信息，比如依赖Python相关信息；若想要日志体现atc进程相关信息，则[--log](../CLI_options/--log.md)设置为除null以外的其他取值。

            - $HOME/ascend/log/**run**/plog/plog-_pid_\_\*.log：运行日志。
            - $HOME/ascend/log/**security**/plog/plog-_pid_\_\*.log：安全日志。

            *pid*代表进程ID，“\*”表示该日志文件创建时的时间戳。

        <!-- end id1 -->
        <!-- @ref: ge/res/docs/zh/user_guides/atc_tools/overview/environment_setup_res.md#id2 -->

        - **日志打屏**：

            atc命令执行过程中，日志默认不打屏，如需打屏显示，则请：

            - 在执行atc命令的当前窗口设置打屏环境变量：

                ```bash
                export ASCEND_SLOG_PRINT_TO_STDOUT=1
                ```

            - atc模型转换命令中，设置[--log](../CLI_options/--log.md)参数（不能设置为null）。

            关于日志的更多信息请参见《日志参考》。

        - **日志重定向**：

            如果不想日志落盘，而是重定向到文件，则模型转换前需要设置上述的日志打屏环境变量，并且atc命令需要设置[--log](../CLI_options/--log.md)参数（不能设置为null），样例如下，xxx请替换为实际参数。

            ```bash
            atc xxx --log=debug >log.txt
            ```

    2. 开启**算子并行编译**功能。

        若网络模型较大，模型转换过程中，可设置如下环境变量，开启算子的并行编译功能。

        ```bash
        export TE_PARALLEL_COMPILER=xxx
        ```

        环境变量TE\_PARALLEL\_COMPILER的值代表算子编译进程数（配置为整数），_xx_请替换为具体整体，取值范围为1\~32，默认值为8；当取值大于1时开启算子的并行编译功能，建议不超过：CPU核数\*80%/AI处理器个数。其中AI处理器个数查询方法如下：

        在安装AI处理器的环境中执行“**npu-smi info -l**”命令，回显信息中的Total Count即为对应的个数。

    3. 打印模型转换过程中**各阶段的图描述信息**。

        ```bash
        export DUMP_GE_GRAPH=1
        ```

        上述环境变量控制dump图的内容多少，取值如下：

        - 1：包含连边关系和数据信息的完整版dump。
        - 2：不含有权重等数据的基本版dump。
        - 3：只显示节点关系的精简版dump。

        设置上述环境变量后，还可以设置如下环境变量，控制dump图的个数。

        ```bash
        export DUMP_GRAPH_LEVEL=2
        ```

        此环境变量只有在DUMP\_GE\_GRAPH开启时才生效，并且默认为2；支持如下两种配置方式，两种方式均是控制图落盘的个数，用户可以按需使用，注意两种配置方式不支持混合使用：

        - 配置数值，取值如下：
            - 1：dump所有阶段的图。
            - 2：dump白名单阶段的图。具体白名单图请参见[表1](../references/dump_graph_details.md)中的"是否白名单"列。
            - 3：dump最后的生成图，即经过GE（Graph Engine，图引擎）优化、编译后的图。
            - 4：dump最早的生成图，即经过GE解析映射算子后，给到软件栈的编译入口图，此时图结构尚未经过GE的编译优化。

        - 配置按照|分隔的字符串，配置如下：

            例如配置为"aa|bb"，则表示dump出名称包含aa和bb的图，aa和bb需要指定为图编译流程中的合法字符串，合法字符串的获取可以从全量的dump图得到。

        设置上述环境变量后，在执行atc命令的当前路径会生成如下文件：

        - ge\_onnx\*.pbtxt：基于ONNX的模型描述结构，可以使用Netron等可视化软件打开。
        - ge\_proto\*.txt：protobuf格式存储的文本文件，该文件可以转成JSON格式文件方便用户定位问题。该文件与ge\_onnx\*.pbtxt一般成对出现，但是ge\_proto\*.txt比ge\_onnx\*.pbtxt文件会多string类型的属性信息，因此ge\_proto\*.txt显示得更完整，用户选择其中一种文件打开。

            由于ge\_proto\*.txt文件结构相比ge\_onnx\*.pbtxt已经做了文件大小的优化，因此DUMP\_GE\_GRAPH环境变量设置为2或3，对ge\_proto\*.txt文件效果相同，都显示为不含有权重等数据的基本版dump。

        - ge\_readable\*.txt：（可选）类似Dynamo fx图风格的高可读性文本文件。该类型的文件只在设置了DUMP\_GRAPH\_FORMAT环境变量，且取值中包括“readable”才会生成。

        上述每个文件对应模型编译过程中的一个步骤，每个文件中包括完成该步骤所涉及的所有算子，关于dump图的详细信息请参见[dump图详细信息](../references/dump_graph_details.md)。

    4. **更多可选环境变量请参见**[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。
