# 编译可执行文件

请以ATC软件包安装用户进行如下操作：

1. 从[https://github.com/tensorflow/tensorflow/archive/v1.15.0.tar.gz](https://github.com/tensorflow/tensorflow/archive/v1.15.0.tar.gz)链接下载Tensorflow源码，然后将下载的源码上传到ATC工具所在Linux服务器任意目录。
2. 登录Linux服务器，切换到Tensorflow源码所在路径，执行如下命令解压源码包：

    ```bash
    tar -zxvf tensorflow-1.15.0.tar.gz
    ```

3. 进入解压后的tensorflow-1.15.0，安装补丁。

    参见[获取xlacompile.patch补丁文件](get_xlacompile_patch.md)获取**xlacompile.patch**补丁，然后上传到Linux服务器tensorflow-1.15.0路径下，执行如下命令安装补丁：

    ```bash
    patch -p1 < xlacompile.patch
    ```

4. 切换到tensorflow-1.15.0目录，执行如下命令编译**xlacompile**工具：

    ```bash
    bazel build --config=monolithic //tensorflow/compiler/aot:xlacompile
    ```

    若出现类似如下信息，则说明编译成功，编译大约需要10分钟左右；若编译失败，请参见[使用bazel编译工具编译时提示“An error occurred during the fetch of repository 'io\_bazel\_rules\_docker'”，编译失败](./bazel_compile_error.md)解决。

    ```console
    Target //tensorflow/compiler/aot:xlacompile up-to-date:
      bazel-bin/tensorflow/compiler/aot/xlacompile
    INFO: Elapsed time: 214.550s, Critical Path: 73.38s
    INFO: 1511 processes: 1511 local.
    INFO: Build completed successfully, 1513 total actions
    ```

    编译成功后，会在$HOME/.cache/bazel/\_bazel\__test_/_abd37aaac8a380ca5a3f13938322fcb2_/external/org\_tensorflow/bazel-out/k8-opt/bin/tensorflow/compiler/aot路径生成**xlacompile**可执行文件（该路径只是样例，请以用户实际编译后的为准）。

    **xlacompile**可执行文件用于将控制流算子的网络模型转成函数类算子的网络模型。

5. 切换到tensorflow-1.15.0目录，执行如下命令编译**summarize\_graph**工具：

    ```bash
    bazel build --config=monolithic -c opt //tensorflow/tools/graph_transforms:summarize_graph
    ```

    若出现类似如下信息，则说明编译成功：

    ```console
    Target //tensorflow/tools/graph_transforms:summarize_graph up-to-date:
      bazel-bin/tensorflow/tools/graph_transforms/summarize_graph
    INFO: Elapsed time: 70.474s, Critical Path: 53.16s
    INFO: 1028 processes: 1028 local.
    INFO: Build completed successfully, 1053 total actions
    ```

    编译成功后，会在$HOME/.cache/bazel/\_bazel\__test_/_abd37aaac8a380ca5a3f13938322fcb2_/execroot/org\_tensorflow/bazel-out/k8-opt/bin/tensorflow/tools/graph\_transforms路径生成**summarize\_graph**可执行文件（该路径只是样例，请以用户实际编译后的为准）。

    **summarize\_graph**可执行文件用来查看有控制流算子网络模型的输入输出节点，方便用户构造config.pbtxt输入输出配置文件。
