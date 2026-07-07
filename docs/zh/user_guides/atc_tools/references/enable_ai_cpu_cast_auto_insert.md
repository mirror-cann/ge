# 开启AI CPU Cast算子自动插入特性

## 简介

模型编译时，若遇到AI CPU算子不支持某种数据类型导致编译失败的场景，可通过启用Cast算子自动插入特性快速将输入转换为算子支持的数据类型，从而实现网络的快速打通。

如[图1](#fig1)，表示MatrixInverse算子的输入x不支持float16的数据类型。

**图 1**  报错示例<a id="fig1"></a>
![](../figures/error_example.png "报错示例")

此种场景下，即可开启Cast算子自动插入特性，详细操作方法见[操作步骤](#操作步骤)。

## 操作步骤

1. 打开AutoCast开关。

    修改“$\{INSTALL\_DIR\}/lib64/plugin/opskernel/config/init.conf”文件，将“AutoCastMode”参数的值修改为1，如下所示：

    ```textproto
    ...
    AutoCastMode = 1
    ```

2. 修改对应的算子信息库（内置AI CPU算子信息库存储在opp安装目录下的“built-in/op\_impl/aicpu/aicpu\_kernel/config”目录下），在需要修改的算子中插入Cast转换规则。

    如下所示，MatrixInverse算子的输入x不支持float16，算子信息库配置如下：

    ```json
    "MatrixInverse":{
            "input0":{
                "name":"x",
                "type":"DT_FLOAT,DT_DOUBLE,DT_COMPLEX128,DT_COMPLEX64"
            },
            "opInfo":{
                "computeCost":"100",
                "engine":"DNN_VM_AICPU",
                "flagAsync":"False",
                "flagPartial":"False",
                "formatAgnostic":"False",
                "opKernelLib":"TFKernel",
                "opsFlag":"OPS_FLAG_OPEN",
                "subTypeOfInferShape":"1"
            },
            "output0":{
                "name":"y",
                "type":"DT_FLOAT,DT_DOUBLE,DT_COMPLEX128,DT_COMPLEX64"
            }
        },
    ```

    为了让其支持float16，需要做如下修改：

    1. 对输入信息进行修改，增加支持的数据类型，并增加数据类型转换规则。

        例如，对MatrixInverse算子，输入增加对float16类型的支持，并增加Cast规则，将float16转换为float32，代表在此输入前会插入一个float16到float32的Cast算子。

        ```json
        "input0":{
            "name":"x",
            "type":"DT_FLOAT,DT_DOUBLE,DT_COMPLEX128,DT_COMPLEX64,DT_FLOAT16",
            "srcAutoCastType":"DT_FLOAT16",
            "dstAutoCastType":"DT_FLOAT"
        },
        ```

        - 支持的“type”中增加“DT\_FLOAT16”数据类型，支持的数据类型可参见对应的算子信息库中Cast算子的定义。
        - 增加配置“srcAutoCastType”，代表输入数据的类型。
        - 增加配置“dstAutoCastType” ，代表需要转换成的目标数据类型。

    2. 对输出信息进行修改，增加支持的数据类型，并增加数据类型转换规则。

        例如，对MatrixInverse算子，输出增加对float16类型的支持，并增加Cast规则，将float32转换为float16，代表在此输出后插入一个float32到float16的Cast算子。

        ```json
        "output0":{
            "name":"y",
            "type":"DT_FLOAT,DT_DOUBLE,DT_COMPLEX128,DT_COMPLEX64,DT_FLOAT16",
            "srcAutoCastType":"DT_FLOAT",
            "dstAutoCastType":"DT_FLOAT16"
        }
        ```

        - 支持的“type”中增加“DT\_FLOAT16”数据类型，支持的数据类型可参见对应的算子信息库中Cast算子的定义。
        - 增加配置“srcAutoCastType”，代表输入数据的类型。
        - 增加配置“dstAutoCastType” ，代表需要转换成的目标数据类型。

    > [!NOTE]说明
    >
    >- 若算子的多个输入、多个输出要求具有相同的数据类型，则每个输入、输出都需要按照上述规则进行修改。
    >- 由于插入Cast算子，精度会有一定程度的损失，具体损失大小与转换的数据类型有关。
