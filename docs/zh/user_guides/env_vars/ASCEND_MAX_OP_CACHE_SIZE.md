# ASCEND\_MAX\_OP\_CACHE\_SIZE

## 功能描述

启用算子编译缓存功能时，可通过此环境变量限制某个AI处理器下缓存文件夹的磁盘空间的大小。默认为500，单位为MB。

- 当编译缓存文件的大小超过此环境变量的设置值，且超过半小时缓存文件未被访问时，缓存文件就会老化。
- 若需要关闭编译缓存老化功能，可将环境变量“ASCEND\_MAX\_OP\_CACHE\_SIZE”设置为“-1”，此时访问算子缓存时不会更新访问时间，算子编译缓存不会老化，磁盘空间使用默认大小500MB。

> [!NOTE]说明
>算子编译时，不会因为编译缓存文件大小超过设置值而中断，所以当此环境变量的值设置过小时，会出现实际编译缓存文件大小超过此设置值的情况。

## 配置示例

```bash
export ASCEND_MAX_OP_CACHE_SIZE=500
```

## 使用约束

系统读取缓存磁盘空间大小配置的优先级如下：

算子编译磁盘缓存的目录下的“op\_cache.ini”配置文件 \> ASCEND\_MAX\_OP\_CACHE\_SIZE环境变量 \>  默认值。

即若开发者同时配置了op\_cache.ini文件和环境变量，则优先读取op\_cache.ini文件中的配置项，若op\_cache.ini文件和环境变量都未设置，则读取系统默认值：默认磁盘空间大小为500MB。

关于“op\_cache.ini”配置文件的详细说明可参见[op\_cache.ini文件说明](#op_cacheini文件说明)。

## op\_cache.ini文件说明

业务开启算子编译缓存功能后，会在指定的算子编译磁盘缓存的目录下自动生成op\_cache.ini文件，开发者可通过该配置文件进行缓存磁盘空间大小的配置。若op\_cache.ini文件不存在，可在该路径下手动创建。

不同场景下指定算子编译磁盘缓存目录的方法不同，例如：

- 基于AscendCL接口开发AI应用场景下，可在“aclCompileOpt”接口中通过编译选项ACL\_OP\_COMPILER\_CACHE\_DIR设置。
- Ascend Graph构图场景下，可通过配置参数“ge.op\_compiler\_cache\_dir”设置。
- ATC模型转换场景下，可通过参数“--op\_compiler\_cache\_dir”设置。
- PyTorch框架场景下，可通过环境变量“ACL\_OP\_COMPILER\_CACHE\_DIR”设置，关于“ACL\_OP\_COMPILER\_CACHE\_DIR”的介绍可参见《[Ascend Extension for PyTorch 环境变量参考](https://www.hiascend.com/document/detail/zh/Pytorch/600/apiref/Envvariables/Envir_001.html)》。
- TensorFlow框架场景下，可通过配置参数“op\_compiler\_cache\_dir”设置。

    TF Adapter配置参数“op\_compiler\_cache\_dir”的详细说明可参见：

  - [《TensorFlow 1.15模型迁移》](https://hiascend.com/document/redirect/CannCommunityTfWizard)中的TF Adapter 1.x接口参考 \> session配置 \> session配置参数说明。
  - 《TensorFlow 2.6.5模型迁移》中的TF Adapter 2.x接口参考 \> npu.global\_options \> 配置参数说明。

> [!NOTE]说明
>以上列举场景仅为示例，若无法覆盖您所使用的场景，请查看对应场景的用户手册。

在“op\_cache.ini”文件中，添加如下信息：

```ini
#配置文件格式，必须包含，自动生成的文件中默认包括如下信息，手动创建时，需要填写
[op_compiler_cache]
#限制某个AI处理器下缓存文件的磁盘空间的大小，整数，单位为MB
max_op_cache_size=500
#当磁盘空间不足时，设置需要保留的缓存文件比例，取值范围：[1,100]，单位为百分比；例如80表示磁盘空间不足时，会保留80%的缓存空间中的文件，其余删除
remain_cache_size_ratio=80
```

## 支持的型号

全量芯片支持
