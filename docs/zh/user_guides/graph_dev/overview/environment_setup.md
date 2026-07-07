# 环境准备

进行开发之前，需要先搭建相应环境，包括安装软件包以及设置环境变量等，本章节给出详细介绍。

## 安装驱动固件与CANN软件包

准备带有AI处理器的硬件环境，参见[《CANN 软件安装》](https://hiascend.com/document/redirect/CannCommunityInstSoftware)完成环境搭建，并确保已安装CANN Toolkit开发套件包和ops算子包（**针对8.5.0及之后版本**，编译Graph为离线模型时，必须安装与目标AI处理器相匹配的ops算子包，否则会导致编译失败），安装完成后：

- “$\{INSTALL\_DIR\}/**opp/built-in/op\_graph/inc**”下提供了CANN算子原型定义，用于通过算子原型构建Graph。
- “$\{INSTALL\_DIR\}/**include/graph**”下提供了Graph构建接口。
- “$\{INSTALL\_DIR\}/**include/ge**”下提供了Graph运行接口。
- “$\{INSTALL\_DIR\}/**_<arch\>_-linux/devlib**”下为相关依赖库。_<arch\>_为操作系统架构。

其中，$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

## 设置环境变量

- **必选环境变量**

    安装CANN软件后，使用CANN运行用户进行编译、运行时，需要以CANN运行用户登录环境，执行如下环境变量：

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

    其中，/usr/local/Ascend/为root用户的默认安装路径，请根据实际情况进行替换。

- **可选环境变量**

    若开发者期望程序编译运行过程中产生的文件存储到统一目录，可通过环境变量ASCEND\_CACHE\_PATH与ASCEND\_WORK\_PATH分别设置共享文件的存储路径与进程独享文件的存储路径。

    ```bash
    export ASCEND_CACHE_PATH=/repo/task001/cache
    export ASCEND_WORK_PATH=/repo/task001/172.16.1.12_01_03
    ```

    关于环境变量ASCEND\_CACHE\_PATH与ASCEND\_WORK\_PATH的使用约束以及落盘文件说明，可参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

>[!NOTE]说明
>
>- 使用export方式设置环境变量后，环境变量只在当前窗口有效，用户可以按需将以上命令写入环境变量配置文件（如.bashrc文件）。
>- 配置可选环境变量前，请使用**env**命令查询ASCEND\_CACHE\_PATH与ASCEND\_WORK\_PATH环境变量是否已存在，建议系统各功能使用统一的规划路径。
