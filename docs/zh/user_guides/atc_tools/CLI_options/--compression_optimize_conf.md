# --compression\_optimize\_conf

## 产品支持情况

<!-- npu="IPV350" id1 -->
- IPV350：不支持
<!-- end id1 -->

<!-- npu="950" id2 -->
- Ascend 950PR/Ascend 950DT：不支持
<!-- end id2 -->

<!-- npu="A3" id3 -->
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id3 -->

<!-- npu="910b" id4 -->
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id4 -->

<!-- npu="310b" id5 -->
- Atlas 200I/500 A2 推理产品：支持
<!-- end id5 -->

<!-- npu="310p" id6 -->
- Atlas 推理系列产品：支持
<!-- end id6 -->

<!-- npu="910" id7 -->
- Atlas 训练系列产品：支持
<!-- end id7 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--compression_optimize_conf_res.md#id1 -->

## 功能说明

压缩优化功能配置文件路径以及文件名，通过该参数开启配置文件中指定的压缩优化特性，从而提升网络性能。

## 关联参数

若通过该参数配置了**calibration**量化特性，则不能再使用高精度特性，比如不能再通过[--precision\_mode](--precision_mode.md)参数配置**force\_fp32**或**must\_keep\_origin\_dtype（原图fp32输入）**；不能再通过[--precision\_mode\_v2](--precision_mode_v2.md)参数配置**origin**；不能通过[--op\_precision\_mode](--op_precision_mode.md)配置**high\_precision**参数等。在高精度模式下设置量化参数，既拿不到量化的性能收益，也拿不到高精度模式的精度收益。

## 参数取值

**参数值：**
配置文件路径以及文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**

当前仅支持配置如下两种压缩方式，用户根据实际情况决定配置哪种压缩方式：

<!-- npu="A3,910b,910,310p,310b" id8 -->
```textproto
enable_first_layer_quantization: true
calibration:
{
    input_data_dir: ./data.bin,d2.bin
    input_shape: in:16,16;in1:16,16
    config_file: simple_config.cfg
    infer_soc: xxxxxx
    infer_aicore_num: 10
    infer_device_id: 0
    infer_ip: x.x.x.x
    infer_port: 8000
    log: info
}
```
<!-- end id8 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--compression_optimize_conf_res.md#id3 -->

其中：

<!-- npu="310p,310b" id9 -->
- **enable\_first\_layer\_quantization**：用于控制AIPP首层卷积是否进行优化（AIPP会与量化后模型首层卷积Conv2D前的Quant算子进行融合），配置文件中冒号前面表示压缩优化特性名称，冒号后面表示是否开启该特性，true表示开启，false表示关闭，默认关闭。

    **开启enable\_first\_layer\_quantization**特性时，只有网络结构中存在AIPP+Conv2D结构，并且在atc命令中将[--enable\_small\_channel](--enable_small_channel.md)参数设置为1时，才有可能获得性能收益。由于量化后的模型存在一定程度上的精度损失，用户根据实际情况决定是否开启该特性。

    只有Atlas 推理系列产品和Atlas 200I/500 A2 推理产品支持该特性。
<!-- end id9 -->

- **calibration**：训练后量化，是指在模型训练结束之后进行的量化，对训练后模型中的权重由浮点数（当前支持float32/float16）量化到低比特整数（比如int8），并通过少量校准数据基于推理过程对数据（activation）进行校准量化，进而加速模型推理速度。训练后量化简单易用，只需少量校准数据，适用于追求高易用性和缺乏训练资源的场景。训练后量化的样例请单击[Link](https://gitee.com/ascend/samples/tree/master/python/level1_single_api/9_amct/atc)获取。

    各参数说明如下，

  - input\_data\_dir：必选配置，模型输入校准数据的bin文件路径。若模型有多个输入，则多个输入的bin数据文件以英文逗号分隔。校准数据集用来计算量化参数，获取校准集时应该具有代表性，推荐使用测试集的子集作为校准数据集。校准数据的bin文件的生成方式可以参考[链接](https://gitee.com/ascend/samples/blob/master/python/level1_single_api/9_amct/amct_caffe/cmd/src/process_data.py)。

  - input\_shape：必选配置，模型输入校准数据的shape信息，例如：input\_name1:n1,c1,h1,w1;input\_name2:n2,c2,h2,w2，节点中间使用英文分号分隔。

  - config\_file：可选配置，训练后量化简易配置文件，该文件配置示例以及参数解释请参见[简易配置文件](../references/quantization_simple_config.md)。

  - infer\_soc：必选配置，进行训练后量化校准推理时，所使用的芯片名称，查询方法请参见[参数取值](../CLI_options/--soc_version.md)。

  - infer\_aicore\_num：可选配置，进行训练后量化校准推理时，使用的AI Core数目，查询方法请参见[--aicore\_num](--aicore_num.md)。

  - infer\_device\_id：可选配置，进行训练后量化校准推理时所使用AI处理器设备的ID，默认为0。

  <!-- npu="310b" id10 -->
  - infer\_ip：Atlas 200I/500 A2 推理产品Ascend RC场景必选，NCS软件包所在服务器IP地址。

  - infer\_port：Atlas 200I/500 A2 推理产品Ascend RC场景必选，NCS软件包所在服务器端口。
  <!-- end id10 -->

  <!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--compression_optimize_conf_res.md#id2 -->

  - log：可选配置，设置训练后量化时的日志等级，该参数只控制训练后量化过程中显示的日志级别，默认显示info级别：

    - debug：输出debug/info/warning/error/event级别的日志信息。
    - info：输出info/warning/error/event级别的日志信息。
    - warning：输出warning/error/event级别的日志信息。
    - error：输出error/event级别的日志信息。

    此外，训练后量化过程中的日志打屏以及日志落盘信息由**AMCT\_LOG\_DUMP**环境变量进行控制：

    - **export AMCT\_LOG\_DUMP=1**：日志打印到屏幕，并落盘到当前路径的amct\_log\__时间戳_/amct\_acl.log文件中，不保存量化因子record文件和graph文件。
    - **export AMCT\_LOG\_DUMP=2**：将日志落盘到当前路径的amct\_log\__时间戳_/amct\_acl.log文件中，并保存量化因子record文件。
    - **export AMCT\_LOG\_DUMP=3**：将日志落盘到当前路径的amct\_log\__时间戳_/amct\_acl.log文件中，并保存量化因子record文件和graph文件。

        为防止日志文件、record文件、graph文件持续落盘导致磁盘被写满，请及时清理这些文件。

        如果用户配置了**ASCEND\_WORK\_PATH**环境变量，则上述日志、量化因子record文件和graph文件存储到该环境变量指定的路径下，例如ASCEND\_WORK\_PATH=/home/test，则存储路径为：/home/test/amct\_acl/amct\_log\__\{pid\}_\__时间戳_。其中，amct\_acl模型转换过程中会自动创建，\{pid\}为进程号。

        > [!NOTE]说明
        >
        >上述日志文件、record文件、graph文件重新执行量化时会被覆盖，请用户自行进行保存。此外，由于生成的日志文件大小和所要量化模型层数有关，请用户确保ATC工具所在服务器有足够空间：
        >以量化resnet101模型为例，日志级别设置为INFO，日志文件大小为12KB左右，中间临时文件大小为260MB左右；日志级别设置为DEBUG，日志文件大小为390KB左右，中间临时文件大小为430MB左右。

**参数默认值：**
无。

## 推荐配置及收益

无

## 示例

假设压缩优化功能配置文件名称为compression\_optimize.cfg，文件内容配置示例如下：

```textproto
<!-- npu="310p,310b" id11 -->
enable_first_layer_quantization: true
<!-- end id11 -->
calibration:
{
    input_data_dir: ./data.bin,d2.bin
    input_shape: in:16,16;in1:16,16
    config_file: simple_config.cfg
    infer_soc: xxxxxx
    infer_aicore_num: 10
    infer_device_id: 0
    infer_ip: x.x.x.x
    infer_port: 8000
    log: info
}
```

将该文件上传到ATC工具所在服务器，例如上传到_$HOME/module_，使用示例如下：

```bash
atc --compression_optimize_conf=$HOME/module/compression_optimize.cfg ...
```

开启量化功能后，模型转换时提示“build\_main build graph\[infer\_graph\_info\] failed”，请参见[开启量化功能，模型转换时提示“build\_main build graph\[infer\_graph\_info\] failed”](../FAQ/quantization_build_error.md)。

## 依赖约束

- 使用该参数的压缩特性时，需要单独安装AMCT（acl）软件包，该包的获取以及安装方法请参见[《AMCT模型压缩工具》](https://hiascend.com/document/redirect/CannCommunityToolAmct)中的“准备环境 \> 获取软件包、上传软件包、安装工具”章节。
- 使用该参数中的**enable\_first\_layer\_quantization**特性时，请确保使用的模型是由AMCT进行量化操作后输出的部署模型。
<!-- npu="A3,910b,910,310p,310b" id13 -->
- 使用配置文件中的**calibration**训练后量化功能时，只支持**带NPU设备**的安装场景，详细介绍请参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)手册搭建对应产品环境。
<!-- end id13 -->
<!-- npu="310b" id12 -->
- Atlas 200I/500 A2 推理产品Ascend RC场景，还需要在运行环境上安装NCS软件，并配置密钥证书，请参见[《AOE调优工具》](https://hiascend.com/document/redirect/CannCommunityToolAoe)>AOE工具（Ascend RC）>环境准备。
<!-- end id12 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--compression_optimize_conf_res.md#id4 -->

- 使用配置文件中的calibration进行训练后量化功能时，ATC工具会调用AMCT量化接口执行相关操作，原理图如下：

    **图 1**  训练后量化原理简图
    ![图示](../figures/ptq_diagram.png "训练后量化原理简图")
