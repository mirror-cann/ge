# 转换模型

下面以Switch\_v1.pb网络模型为例进行说明，演示如何将有控制流算子网络模型转换成函数类算子网络模型，然后通过ATC工具转换成适配AI处理器的离线模型。请先参见[获取Switch\_v1.pb网络模型](./get_switch_v1_pb_model.md)获取Switch\_v1.pb网络模型。

1. 获取控制流算子网络模型的输出。

    切换到**summarize\_graph**可执行文件所在路径，执行如下命令：

    ```bash
    ./summarize_graph --in_graph=/home/test/module/Switch_v1.pb
    ```

    若返回如下信息，则说明执行成功：

    ```console
    Found 3 possible inputs: (name=x, type=float(1), shape=<unknown>) (name=y, type=float(1), shape=<unknown>) (name=z, type=float(1), shape=<unknown>)
    No variables spotted.
    Found 1 possible outputs: (name=add, op=AddV2)
    Found 0 (0) const parameters, 0 (0) variable parameters, and 0 control_edges
    Op types used: 3 Placeholder, 3 Switch, 2 AddV2, 2 Mul, 2 Square, 1 Identity, 1 Less, 1 Merge
    To use with tensorflow/tools/benchmark:benchmark_model try these arguments:
    bazel run tensorflow/tools/benchmark:benchmark_model -- --graph=/home/test/module/Switch_v1.pb --show_flops --input_layer=x,y,z --input_layer_type=float,float,float --input_layer_shape=:: --output_layer=add
    ```

2. 构造config.pbtxt输出配置文件。

    在任意路径执行**vim config.pbtxt**命令创建config.pbtxt文件，本示例以在$HOME/module路径创建为例进行说明。

    根据步骤1所示，该网络模型有一个输出，构造的config.pbtxt配置文件样例如下（如下配置文件只是样例，需要用户根据实际情况修改输出算子的name，其中fetch表示输出）：

    ```pbtxt
    fetch {
      id { node_name: "add" }
    }
    ```

    配置完成后，保存文件并退出。

3. 生成函数类算子网络模型的配置文件。

    在任意路径执行如下命令设置**xlacompile**命令执行过程中的打屏日志信息：

    ```bash
    export TF_CPP_MIN_LOG_LEVEL=0
    export TF_CPP_MIN_VLOG_LEVEL=1
    ```

    切换到**xlacompile**可执行文件所在路径，执行如下命令：

    ```bash
    ./xlacompile --graph=/home/test/module/Switch_v1.pb --config=/home/test/module/config.pbtxt --output=/home/test/module/Switch_v1_v2
    ```

    如果提示“Successfully convert ...”信息，则说明转换成功。切换到**--output**参数指定的路径，可以看到如下输出文件：

    ```console
    -rw-rw-r-- 1 test test 1236 Jun 20 17:13 Switch_v1_v2.pb
    -rw-rw-r-- 1 test test 4803 Jun 20 17:13 Switch_v1_v2.pbtxt
    ```

4. 函数类算子网络模型生成graph子图。后续使用ATC工具进行模型转换时，需要使用该文件生成子图。

    在任意路径执行如下命令，将函数类算子网络模型生成子图：

    ```python
    python3.7.5 ${INSTALL_DIR}/compiler/python/func2graph/func2graph.py -m /home/test/module/Switch_v1_v2.pb
    ```

    若提示如下信息，则说明生成成功。

    ```console
    graph_def_file:  /home/test/module/graph_def_library.pbtxt
    INFO: Convert to subgraphs successfully.
    ```

5. 函数类算子网络模型转换成适配昇腾AI处理器的离线模型。

    参见[设置环境变量](../overview/environment_setup.md#设置环境变量)设置ATC工具执行时需设置的环境变量，然后执行如下命令进行模型转换：

    ```bash
    atc --model=/home/test/module/Switch_v1_v2.pb --framework=3 --output=/home/test/module/out/Switch_v1_v2_to_om --soc_version=<soc_version>
    ```

    若提示如下信息，则说明模型转换成功：

    ```console
    ATC run success, welcome to the next use.
    ```

    成功执行命令后，在**--output**参数指定的路径下，可查看模型文件。
