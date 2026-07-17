# ASCEND\_ENGINE\_PATH

## 功能描述

单算子JSON文件转换成离线模型场景，如果希望模型转换时只使用TBE算子（不查找AI CPU算子，找不到TBE算子则报错），则需要使用该环境变量。

设置该环境变量后，如果用户想要执行其他操作，需要删除上述环境变量：执行**unset ASCEND\_ENGINE\_PATH**命令，使其失效。

## 配置示例

```bash
export ASCEND_ENGINE_PATH=${INSTALL_DIR}/lib64/plugin/opskernel/libfe.so:${INSTALL_DIR}/lib64/plugin/opskernel/libge_local_engine.so:${INSTALL_DIR}/lib64/plugin/opskernel/librts_engine.so
```

其中$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

## 使用约束

该环境变量仅适用于将算子json文件转换为离线om模型的场景。

## 产品支持情况

全量芯片支持
