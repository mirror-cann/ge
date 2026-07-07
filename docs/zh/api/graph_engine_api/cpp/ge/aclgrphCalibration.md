# aclgrphCalibration

## 产品支持情况

<!-- npu="950" id1 -->
- Ascend 950PR/Ascend 950DT：支持
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
- IPV350：支持
<!-- end id7 -->
<!-- @ref: ge/res/docs/zh/api/graph_engine_api/cpp/ge/aclgrphCalibration_res.md#id1 -->

## 头文件/库文件

- 头文件：\#include <amct/acl\_graph\_calibration.h\>
- 库文件：libamctacl.so

## 功能说明

将非量化Graph自动修改为量化后的Graph。详细使用场景请参见[量化](../../../../user_guides/graph_dev/more_features/quantization.md)。

## 函数原型

```c++
graphStatus aclgrphCalibration(ge::Graph &graph, const std::map<ge::AscendString, ge::AscendString> &quantizeConfigs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 待修改的用户原始Graph。 |
| quantizeConfigs | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述执行接口所需要的配置选项。具体配置参数如下所示:<br><br>  - INPUT_DATA_DIR：必填，用于计算量化因子的数据bin文件路径，建议传入不少于一个batch的数据。如果模型有多个输入，则输入数据文件以英文逗号分隔。<br>  - INPUT_SHAPE：必填，输入数据的shape。例如："input_name1:n1,c1,h1,w1;input_name2:n2,c2,h2,w2"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input_name必须是Graph中的节点名称。<br>  - SOC_VERSION：必填，进行训练后量化校准推理时，所使用的芯片名称。<soc_version>查询方法为：<br>针对如下产品：在安装AI处理器的服务器执行npu-smi info命令进行查询，获取Name信息。实际配置值为AscendName，例如Name取值为xxxyy，实际配置值为Ascendxxxyy。Atlas A2 训练系列产品/Atlas A2 推理系列产品<br>Atlas 200I/500 A2 推理产品<br>Atlas 推理系列产品<br>Atlas 训练系列产品<br>针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行npu-smi info -t board -i id -c chip_id命令进行查询，获取Chip Name和NPU Name信息，实际配置值为Chip Name_NPU Name。例如Chip Name取值为Ascendxxx，NPU Name取值为1234，实际配置值为Ascendxxx_1234。其中：id：设备id，通过npu-smi info -l命令查出的NPU ID即为设备id。chip_id：芯片id，通过npu-smi info -m命令查出的Chip ID即为芯片id。<br>针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行npu-smi info -t board -i id命令进行查询，获取Chip Name和NPU Name信息，实际配置值为Chip Name_NPU Name。例如Chip Name取值为Ascendxxx，NPU Name取值为1234，实际配置值为Ascendxxx_1234。其中，id为设备id，通过npu-smi info -l命令查出的NPU ID即为设备id。<br><br>BS9SX1A AI处理器参数值：BS9SX1AA、BS9SX1AB、BS9SX1AC<br>BS9SX2A AI处理器参数值：BS9SX2A<br>IPV350参数值：Ascend035<br>  - 针对如下产品：在安装AI处理器的服务器执行npu-smi info命令进行查询，获取Name信息。实际配置值为AscendName，例如Name取值为xxxyy，实际配置值为Ascendxxxyy。Atlas A2 训练系列产品/Atlas A2 推理系列产品<br>Atlas 200I/500 A2 推理产品<br>Atlas 推理系列产品<br>Atlas 训练系列产品<br>  - 针对Atlas A3 训练系列产品/Atlas A3 推理系列产品，在安装AI处理器的服务器执行npu-smi info -t board -i id -c chip_id命令进行查询，获取Chip Name和NPU Name信息，实际配置值为Chip Name_NPU Name。例如Chip Name取值为Ascendxxx，NPU Name取值为1234，实际配置值为Ascendxxx_1234。其中：id：设备id，通过npu-smi info -l命令查出的NPU ID即为设备id。chip_id：芯片id，通过npu-smi info -m命令查出的Chip ID即为芯片id。<br>  - id：设备id，通过npu-smi info -l命令查出的NPU ID即为设备id。<br>  - chip_id：芯片id，通过npu-smi info -m命令查出的Chip ID即为芯片id。<br>  - 针对Ascend 950PR/Ascend 950DT，在安装AI处理器的服务器执行npu-smi info -t board -i id命令进行查询，获取Chip Name和NPU Name信息，实际配置值为Chip Name_NPU Name。例如Chip Name取值为Ascendxxx，NPU Name取值为1234，实际配置值为Ascendxxx_1234。其中，id为设备id，通过npu-smi info -l命令查出的NPU ID即为设备id。<br>  - INPUT_FORMAT：选填，输入数据的排布格式，支持"NCHW"、"NHWC"、"ND"。<br>  - INPUT_FP16_NODES：选填，指定输入数据类型为FP16的输入节点名称。<br>  - CONFIG_FILE：选填，用于配置高级选项的配置文件路径。配置文件的示例请参考[量化简易配置文件](../../../../user_guides/graph_dev/appendix/quantization_config.md#量化简易配置文件)。<br>  - LOG_LEVEL：选填，设置训练后量化时的日志等级，该参数只控制训练后量化过程中显示的日志级别，默认显示info级别：debug：输出debug/info/warning/error/event级别的日志信息。info：输出info/warning/error/event级别的日志信息。warning：输出warning/error/event级别的日志信息。error：输出error/event级别的日志信息。<br>此外，训练后量化过程中的日志落盘信息由AMCT_LOG_DUMP环境变量进行控制：export AMCT_LOG_DUMP=1：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，不保存量化因子record文件和graph文件。export AMCT_LOG_DUMP=2：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，同时在“amct_log_{timestamp}”目录下保存量化因子record文件。export AMCT_LOG_DUMP=3：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，同时在“amct_log_{timestamp}”目录下保存量化因子record文件和包含量化过程中各阶段图描述信息的graph文件。<br><br>为防止日志文件、record文件、graph文件持续落盘导致磁盘被写满，请及时清理这些文件。<br>如果用户配置了ASCEND_WORK_PATH环境变量，则上述日志、量化因子record文件和graph文件存储到该环境变量指定的路径下，例如ASCEND_WORK_PATH=/home/test，则存储路径为：/home/test/amct_acl/amct_log_{pid}_时间戳。其中，amct_acl模型转换过程中会自动创建，{pid}为进程号。<br> 说明： 上述日志文件、record文件、graph文件重新执行量化时会被覆盖，请用户自行进行保存。此外，由于生成的日志文件大小和所要量化模型层数有关，请用户确保服务器有足够空间：<br>以量化resnet101模型为例，日志级别设置为INFO，日志文件大小为12KB左右，中间临时文件大小为260MB左右；日志级别设置为DEBUG，日志文件大小为390KB左右，中间临时文件大小为430MB左右。<br>  - debug：输出debug/info/warning/error/event级别的日志信息。<br>  - info：输出info/warning/error/event级别的日志信息。<br>  - warning：输出warning/error/event级别的日志信息。<br>  - error：输出error/event级别的日志信息。<br>  - export AMCT_LOG_DUMP=1：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，不保存量化因子record文件和graph文件。<br>  - export AMCT_LOG_DUMP=2：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，同时在“amct_log_{timestamp}”目录下保存量化因子record文件。<br>  - export AMCT_LOG_DUMP=3：日志落盘到当前路径的“amct_log_{timestamp}/amct_acl.log”文件中，同时在“amct_log_{timestamp}”目录下保存量化因子record文件和包含量化过程中各阶段图描述信息的graph文件。<br>  - OUT_NODES：选填，用户Graph的输出节点名。<br>  - DEVICE_ID：选填，指定设备ID，默认为0。Ascend 950PR/Ascend 950DT，支持该选项。Atlas 训练系列产品，支持该选项。Atlas 推理系列产品，支持该选项。Atlas 200I/500 A2 推理产品，支持该选项。Atlas A2 训练系列产品/Atlas A2 推理系列产品，支持该选项。Atlas A3 训练系列产品/Atlas A3 推理系列产品，支持该选项。BS9SX1A AI处理器，不支持该选项。BS9SX2A AI处理器，不支持该选项。<br>  - Ascend 950PR/Ascend 950DT，支持该选项。<br>  - Atlas 训练系列产品，支持该选项。<br>  - Atlas 推理系列产品，支持该选项。<br>  - Atlas 200I/500 A2 推理产品，支持该选项。<br>  - Atlas A2 训练系列产品/Atlas A2 推理系列产品，支持该选项。<br>  - Atlas A3 训练系列产品/Atlas A3 推理系列产品，支持该选项。<br>  - BS9SX1A AI处理器，不支持该选项。<br>  - BS9SX2A AI处理器，不支持该选项。<br>  - infer_aicore_num：必填，进行训练后量化校准推理时，使用的AI Core数目，昇腾610 AI处理器和BS9SX1A AI处理器取值范围为{1,2,4,8,10}；BS9SX2A AI处理器取值范围为{1,2,4}。<br>  - infer_aicore_num：选填，进行训练后量化校准推理时，使用的AI Core核数，查询方法请参见[算子编译与图编译](./aclgrphBuildInitialize_config_params/operator_and_graph_compilation.md) > AICORE_NUM<br>  - groupId：选填，进行训练后量化校准推理时，使用的算力Group ID，取值范围为{0,1,2,3}，默认为0。后续上板推理时，先调用aclrtGetAllGroupInfo接口获取所有Group信息，然后再调用aclrtSetGroup接口指定当前运算使用哪个Group的算力。接口详细说明请参见《Runtime运行时 API》中的“算力Group查询与设置”。<br>  - IP_ADDR：指定NCS所在服务器的IP地址。Ascend 950PR/Ascend 950DT，不支持该选项。Atlas 训练系列产品，不支持该选项。Atlas 推理系列产品，不支持该选项。Atlas 200I/500 A2 推理产品，Ascend RC场景支持该选项，且必填。Atlas A2 训练系列产品/Atlas A2 推理系列产品，不支持该选项。Atlas A3 训练系列产品/Atlas A3 推理系列产品，不支持该选项。BS9SX1A AI处理器，支持该选项。BS9SX2A AI处理器，支持该选项。<br>  - Ascend 950PR/Ascend 950DT，不支持该选项。<br>  - Atlas 训练系列产品，不支持该选项。<br>  - Atlas 推理系列产品，不支持该选项。<br>  - Atlas 200I/500 A2 推理产品，Ascend RC场景支持该选项，且必填。<br>  - Atlas A2 训练系列产品/Atlas A2 推理系列产品，不支持该选项。<br>  - Atlas A3 训练系列产品/Atlas A3 推理系列产品，不支持该选项。<br>  - BS9SX1A AI处理器，支持该选项。<br>  - BS9SX2A AI处理器，支持该选项。<br>  - PORT：指定NCS所在服务器端口。Ascend 950PR/Ascend 950DT，不支持该选项。Atlas 训练系列产品，不支持该选项。Atlas 推理系列产品，不支持该选项。Atlas 200I/500 A2 推理产品，Ascend RC场景支持该选项，且必填。Atlas A2 训练系列产品/Atlas A2 推理系列产品，不支持该选项。Atlas A3 训练系列产品/Atlas A3 推理系列产品，不支持该选项。BS9SX1A AI处理器，支持该选项。BS9SX2A AI处理器，支持该选项。<br>  - Ascend 950PR/Ascend 950DT，不支持该选项。<br>  - Atlas 训练系列产品，不支持该选项。<br>  - Atlas 推理系列产品，不支持该选项。<br>  - Atlas 200I/500 A2 推理产品，Ascend RC场景支持该选项，且必填。<br>  - Atlas A2 训练系列产品/Atlas A2 推理系列产品，不支持该选项。<br>  - Atlas A3 训练系列产品/Atlas A3 推理系列产品，不支持该选项。<br>  - BS9SX1A AI处理器，支持该选项。<br>  - BS9SX2A AI处理器，支持该选项。<br>  - INSERT_OP_CONF：插入算子的配置文件路径与文件名，例如Aipp预处理算子。配置文件路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（_）、中划线（-）、句点（.）、中文字符；文件后缀不局限于.cfg格式，但是配置文件中的内容需要满足prototxt格式。<br>配置文件的内容示例如下：<br>aipp_op {<br>   aipp_mode:static<br>   input_format:YUV420SP_U8<br>   csc_switch:true<br>   var_reci_chn_0:0.00392157<br>   var_reci_chn_1:0.00392157<br>   var_reci_chn_2:0.00392157<br>}<br>关于配置文件详细配置以及参数说明，请参见《ATC离线模型编译工具》>高级功能 > 开启AIPP章节。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他：失败。 |

## 约束说明

- 对于已经被插入量化算子的Graph不支持进行量化。
- 支持量化的层及约束请参考[支持量化的层及约束](../../../../user_guides/graph_dev/appendix/quantization_config.md)。
- 使用配置文件中的**calibration**训练后量化功能时，只支持**带NPU设备**的安装场景，详细介绍请参见《CANN 软件安装》手册搭建对应产品环境。
- Atlas 200I/500 A2 推理产品Ascend RC场景，还需要在运行环境上安装NCS软件，并配置密钥证书，请参见《AOE调优工具》\>AOE工具（Ascend RC）\>环境准备。
- 使用配置文件中的**calibration**训练后量化功能时，只支持**带NPU设备**的安装场景，因此需要在运行环境上安装NCS软件，并配置密钥证书，请参见《AOE调优工具》\>软件安装\>NCS安装和启动（运行环境）。
