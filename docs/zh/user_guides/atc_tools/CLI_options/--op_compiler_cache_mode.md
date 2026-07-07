# --op\_compiler\_cache\_mode

## 产品支持情况

全量芯片支持

## 功能说明

用于配置算子编译磁盘缓存模式。

## 关联参数

如果要自行指定算子编译磁盘缓存的路径，则需要通过[--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)参数指定。

## 参数取值

**参数值：**

- enable：表示启用算子编译缓存。启用后可以避免针对相同编译参数及算子参数的算子重复编译，从而提升编译速度。
- force：启用算子编译缓存功能，区别于enable模式，force模式下会强制刷新缓存，即先删除已有缓存，再重新编译并加入缓存。比如当用户的python变更、依赖库变更、算子调优后知识库变更等，需要先指定为force用于先清理已有的缓存，后续再修改为enable模式，以避免每次编译时都强制刷新缓存。
- disable：（默认值）表示禁用算子编译缓存，算子重新编译。

**参数值约束：**

1. 由于force选项会先删除已有缓存，所以不建议在程序并行编译时设置，否则可能会导致其他模型使用的缓存内容被清除而导致失败。
2. 建议模型最终发布时设置编译缓存选项为disable或者force。
3. 如果算子调优后知识库变更，则需要通过设置为force来刷新缓存，否则无法应用新的调优知识库，从而导致调优应用执行失败。
4. 调试开关打开的场景下：
    - [--op\_debug\_level](--op_debug_level.md)配置非0值：会忽略--op\_compiler\_cache\_mode参数的配置，不启用算子编译缓存功能，算子全部重新编译。
    - [--op\_debug\_config](--op_debug_config.md)配置非空，且**未配置op\_debug\_list字段**，会忽略--op\_compiler\_cache\_mode参数的配置，不启用算子编译缓存功能，算子全部重新编译。
    - [--op\_debug\_config](--op_debug_config.md)配置非空，且**配置文件中配置了op\_debug\_list字段**：
        - 列表中的算子，忽略--op\_compiler\_cache\_mode参数的配置继续重新编译。
        - 列表外的算子，如果--op\_compiler\_cache\_mode参数配置为enable或force，则启用缓存功能；若配置为disable，则不启用缓存功能，仍旧重新编译。

## 推荐配置及收益

推荐配置为enable：启用后可以避免针对相同编译参数及算子参数的算子重复编译，从而提升编译速度。

## 示例

```bash
atc --op_compiler_cache_mode=enable --op_compiler_cache_dir=$HOME/atc_data/kernel_cache --op_debug_level=0 ...
```

## 使用约束

启用算子编译缓存功能时，可以通过**配置文件**（ATC工具运行后，会在[--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)参数指定的路径下自动生成op\_cache.ini配置文件）、**环境变量**两种方式来设置缓存文件夹的磁盘空间大小：

1. 通过配置文件op\_cache.ini设置

    若op\_cache.ini文件不存在，则需要手动创建。打开该文件，增加如下信息：

    ```ini
    #配置文件格式，必须包含，自动生成的文件中默认包括如下信息，手动创建时，需要输入
    [op_compiler_cache]
    #限制某个芯片下缓存文件夹的磁盘空间的大小，默认值为500，取值需为整数，单位为MB
    max_op_cache_size=500
    #设置需要保留缓存的空间大小比例，取值范围：[1,100]，默认值为50，单位为百分比；例如取值为80表示缓存空间不足时，删除缓存文件，保留80%缓存空间
    remain_cache_size_ratio=50
    ```

    - 上述文件中的max\_op\_cache\_size和remain\_cache\_size\_ratio参数取值都有效时，op\_cache.ini文件才会生效。
    - 当编译缓存文件大小超过“max\_op\_cache\_size”的设置值，且超过半小时缓存文件未被访问时，缓存文件就会老化（算子编译时，不会因为编译缓存文件大小超过设置值而中断，所以当“max\_op\_cache\_size”设置过小时，会出现实际编译缓存文件大小超过此设置值的情况）。
    - 若需要关闭编译缓存老化功能，可将“max\_op\_cache\_size”设置为“-1”，此时访问算子缓存时不会更新访问时间，算子编译缓存不会老化，磁盘空间使用默认大小500MB。
    - 若多个使用者使用相同的缓存路径，建议使用配置文件的方式进行设置，该场景下op\_cache.ini文件会影响所有使用者。

2. 通过环境变量设置

    该场景下，开发者可以通过环境变量ASCEND\_MAX\_OP\_CACHE\_SIZE来限制某个芯片下缓存文件夹的磁盘空间的大小，当编译缓存空间大小达到ASCEND\_MAX\_OP\_CACHE\_SIZE设置的取值，且超过半个小时缓存文件未被访问时，缓存文件就会老化。可通过环境变量ASCEND\_REMAIN\_CACHE\_SIZE\_RATIO设置需要保留缓存的空间大小比例。

    配置示例如下：

    ```bash
    # ASCEND_MAX_OP_CACHE_SIZE环境变量默认值为500，取值需为整数，单位为MB
    export ASCEND_MAX_OP_CACHE_SIZE=500
    # ASCEND_REMAIN_CACHE_SIZE_RATIO环境变量取值范围：[1,100]，默认值为50，单位为百分比；例如取值为80表示缓存空间不足时，删除缓存文件，保留80%缓存空间
    export ASCEND_REMAIN_CACHE_SIZE_RATIO=50
    ```

    - 通过环境变量配置，只对当前用户生效。
    - 若需要关闭编译缓存老化功能，可将环境变量“**ASCEND\_MAX\_OP\_CACHE\_SIZE**”设置为“-1”，此时访问算子缓存时不会更新访问时间，算子编译缓存不会老化，磁盘空间使用默认大小500MB。

**若同时配置了op\_cache.ini文件和环境变量，则优先读取op\_cache.ini文件中的配置项，若op\_cache.ini文件和环境变量都未设置，则读取系统默认值：默认磁盘空间大小500MB，默认保留缓存空间的50%。**
