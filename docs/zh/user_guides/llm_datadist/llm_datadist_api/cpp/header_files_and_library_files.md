# 头文件和库文件说明

相关头文件在`${INSTALL_DIR}/include/llm_datadist`目录下，库文件在`${INSTALL_DIR}/lib64/`目录下。${INSTALL_DIR}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**表 1**  头文件列表

| 定义接口的头文件 | 用途 | 对应的库文件 |
| --- | --- | --- |
| llm\_datadist.h | 面向大模型KV Cache传输的场景，提供带有KV Cache语义的接口能力。支持角色管理、链路管理、KV Cache注册、连续KV Cache传输、KV Block传输等功能。 | libllm\_engine.so |
