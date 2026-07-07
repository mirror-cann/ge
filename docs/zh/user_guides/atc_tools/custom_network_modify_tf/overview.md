# 概述

## 简介

本章节介绍如何使用TensorFlow的xlacompile工具，将有控制流算子的网络模型（如[图1](#fig1)所示）转成函数类算子的网络模型（如[图2](#fig2)所示），然后利用ATC工具转换成适配AI处理器的离线模型。

**图 1**  有控制流算子的网络模型<a id="fig1"></a>
![图示](../figures/network_model_with_control_flow_op.png "有控制流算子的网络模型")

**图 2**  函数类算子的网络模型<a id="fig2"></a>
![图示](../figures/function_class_op_network_model.png "函数类算子的网络模型")

## 使用前提

- 确保ATC工具所在服务器能连接网络。
- 安装bazel编译工具。

    请参见[https://docs.bazel.build/versions/master/install-ubuntu.html](https://docs.bazel.build/versions/master/install-ubuntu.html)官方地址安装bazel编译工具。

- 安装TensorFlow以及依赖future。

    ATC安装路径下的func2graph.py脚本依赖TensorFlow，使用**pip3.7.5 list**查看列表中是否有TensorFlow依赖，若有则不用安装，否则执行如下命令安装：

    ```python
    pip3.7.5 install tensorflow==1.15 --user
    ```

    bazel编译工具依赖python，请使用**pip3.7.5 list**命令查看是否安装future、patch，若有则不用安装，否则执行如下命令安装：

    ```python
    pip3.7.5 install future --user
    pip3.7.5 install patch --user
    pip3.7.5 install numpy --user
    ```

    如果执行上述命令时无法连接网络，且提示“Could not find a version that satisfies the requirement xxx”，请参见[使用pip3 install软件时提示" Could not find a version that satisfies the requirement xxx"](pip3_install_error.md)解决。
