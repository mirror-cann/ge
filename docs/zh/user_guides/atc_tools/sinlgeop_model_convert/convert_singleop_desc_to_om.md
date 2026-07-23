# 如何将算子描述文件转成离线模型

本节给出单算子描述文件转成离线模型的详细步骤。

1. 参见[什么是单算子描述文件](singleop_desc_intro.md)中的参数解释以及[配置文件样例](config_file_example.md)构造单算子描述文件。本章节以构造format为ND的Add单算子add.json为例进行说明。
2. 以CANN软件包运行用户，将步骤1构造的单算子描述文件上传到开发环境任意目录，例如$HOME/singleop/目录下。
3. 执行如下命令生成离线模型。（如下命令中使用的目录以及文件均为样例，请以实际为准）

    ```bash
    atc --singleop=$HOME/singleop/add.json --output=$HOME/singleop/out/op_model --soc_version=<soc_version>
    ```

    - --singleop：用于指定_add.json_单算子描述文件。
    - --output：转换后的离线模型存放路径。
    - --soc\_version：AI处理器的型号。

    关于参数的详细解释请参见[参数说明](../CLI_options/README.md)。

4. 若提示如下信息，则说明模型转换成功。

    <!-- npu="950,A3,910b,910,310p,310b" id1 -->
    若模型转换失败，请参见[《故障处理》](https://hiascend.com/document/redirect/CannCommunitytrouble)\>  “错误码参考”章节进行辅助定位。
    <!-- end id1 -->

    ```textconsole
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在output参数指定的路径下，可查看离线模型文件\*.om。
