# 使用pip3 install软件时提示" Could not find a version that satisfies the requirement xxx"

## 问题描述

安装依赖时，使用**pip3 install xxx**命令安装相关软件时提示无法连接网络，且提示"Could not find a version that satisfies the requirement xxx"，使用**apt-get update**命令检查源可用。提示信息如下图所示。

**图 1**  使用pip3.7.5安装软件提示信息
![图示](../figures/pip3_7_5_install_message.png "使用pip3-7-5安装软件提示信息")

## 可能原因

没有配置pip源。

## 解决方法

配置pip源，配置方法如下：

1. 使用ATC软件包安装用户，执行如下命令：

    ```bash
    cd ~/.pip
    ```

    如果提示目录不存在，则执行如下命令创建：

    ```bash
    mkdir ~/.pip
    cd ~/.pip
    ```

    在.pip目录下创建pip.conf文件，命令为：

    ```bash
    touch pip.conf
    ```

2. 编辑pip.conf文件。

    使用**vi pip.conf**命令打开pip.conf文件，写入如下内容后，保存文件并退出：

    ```ini
    [global]
    #可用的源，请根据实际情况进行替换。
    index-url=http://xxx
    [install]
    #可信主机，请根据实际情况进行替换。
    trusted-host=xxx
    ```
