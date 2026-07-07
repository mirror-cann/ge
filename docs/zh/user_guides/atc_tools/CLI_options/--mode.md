# --mode

## 产品支持情况

全量芯片支持

## 功能说明

运行模式。

## 关联参数

- 若[--mode](--mode.md)取值为1或5，则需要与[--om](--om.md)、[--json](--json.md)参数配合使用。如果将原始模型文件转换成带shape信息的JSON文件，则还需要与[--dump\_mode](--dump_mode.md)参数配合使用。
- 若[--mode](--mode.md)取值为6，则只需要与[--om](--om.md)参数配合使用。
- 若[--mode](--mode.md)取值为3，需要自行指定预检结果保存路径时，需要与[--check\_report](--check_report.md)参数配合使用。
<!-- npu="IPV350" id3 -->
- 若[--soc\_version](--soc_version.md)取值为Ascend035，则该参数仅支持配置为30。
<!-- end id3 -->

## 参数取值

**参数值：**

- 0：（默认值）生成适配AI处理器的离线模型，模型文件格式为\*.om。
- 1：离线模型或原始模型文件转JSON文件，方便查看模型中的参数信息。
- 3：仅做预检，检查模型文件的内容是否合法。
- 5：dump图结构文件转JSON文件，用于解析图编译过程中产生的dump图结构（ge\_proto\*.txt格式文件，ge\_onnx\*.pbtxt暂不支持），然后将dump图结构转换成JSON文件，方便用户定位。
- 6：针对已有的**离线模型**，显示模型信息，包括模型占用的关键资源信息、编译与运行环境等信息。
<!-- npu="IPV350" id1 -->
- 30：生成适配AI处理器的离线模型，模型文件格式为\*.exeom，同时生成图调试信息文件\*.dbg，用于dump、profiling的图调试。\*.om模型和\*.exeom模型的对比如下：
  - \*.exeom文件感知具体的硬件调度能力、包含目标执行平台的运行时数据结构（这些数据以二进制的形式保存在\*.exeom文件中），在模型加载阶段，加载恢复二进制内容，根据用户应用程序传递的数据区地址，或实际申请到的数据地址，刷新二进制中的地址指针值后，将二进制内容直接拷贝至Device，达到提升模型加载性能、降低模型加载内存峰值占用的效果。**在一些资源受限的场景，建议使用\*.exeom模型文件，增强产品的商用竞争力。**
  - \*.om文件不感知具体的硬件调度能力、包含中间态的抽象数据结构，在模型加载阶段，再根据具体执行平台的调度特性，生成运行时数据结构。
<!-- end id1 -->

**参数值约束：**

若[--mode](--mode.md)取值为5，需要设置相关环境变量，先获取dump图结构文件，方法请参见[2.c](../overview/environment_setup.md)。设置完环境变量，模型转换完毕，在执行atc命令的当前路径会生成相应的图结构文件。

## 推荐配置及收益

无。

## 示例

- 参数值取值为0：

    ```bash
    atc --mode=0 --framework=3 --model=$HOME/module/resnet50_tensorflow.pb  --output=$HOME/module/out/tf_resnet50  --soc_version=<soc_version>
    ```

- 参数值取值为1：
  - 离线模型转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/out/tf_resnet50.om  --json=$HOME/module/out/tf_resnet50.json
    ```

  - 原始模型文件转换为JSON文件

    ```bash
    atc --mode=1 --om=$HOME/module/resnet50_tensorflow.pb  --json=$HOME/module/out/tf_resnet50.json  --framework=3
    ```

- 参数值取值为3：

    ```bash
    atc --mode=3 --framework=3 --model=$HOME/module/resnet50_tensorflow.pb --soc_version=<soc_version>
    ```

    执行完毕，在当前路径生成预检结果文件check\_result.json。

- 参数值取值为5：

    ```bash
    atc --mode=5 --om=$HOME/module/ge_proto_00000000_PreRunBegin.txt --json=$HOME/module/out/ge_proto.json
    ```

- 参数值取值为6：

    ```bash
    atc --mode=6 --om=$HOME/module/out/tf_resnet50.om
    ```

    命令执行完毕，屏幕会打印类似如下信息：

    ```console
    ============ Display Model Info start ============
    # 模型转换使用的atc命令
    Original Atc command line: ${INSTALL_DIR}/bin/atc.bin --model=$HOME/module/resnet50_tensorflow.pb  --framework=3 --output=$HOME/module/out/tf_resnet50 --soc_version=<soc_version> --display_model_info=1
    # ATC软件版本信息、soc_version版本信息、原始框架信息
    system   info: atc_version[xxx], soc_version[xxx], framework_type[xxx].
    # 运行时的占用内存、权重大小、逻辑stream数目、event数目
    resource info: memory_size[xxx B], weight_size[xxx B], stream_num[xxx], event_num[xxx].
    # 离线模型文件中各分区大小、包括ModelDef、权重、tbe_kernels、task_info、so占用的大小等
    om       info: modeldef_size[xxx B], weight_data_size[xxx B], tbe_kernels_size[xxx B], cust_aicpu_kernel_store_size[xxx B], task_info_size[xxx B], so_store_size[xxx B].
    ============ Display Model Info end   ============
    ```

<!-- npu="IPV350" id2 -->
- 参数值取值为30：

    ```bash
    atc --mode=30 --framework=3 --model=$HOME/module/resnet50_tensorflow.pb  --output=$HOME/module/out/tf_resnet50  --soc_version=<soc_version>
    ```
<!-- end id2 -->

## 依赖约束

无。
