# 算子编译与图编译

## EXCLUDE\_ENGINES

设置图编译时不使用某个或某些加速引擎。多个以“|”分隔。此参数实际对应的options参数为`ge.exec.exclude_engines`。

NPU集成了多种硬件加速器（也叫加速引擎），比如AiCore/AiVec/AiCpu（按照优先级排列）等，在图编译阶段会按照优先级为算子选择合适的引擎，即当同一个算子被多种引擎支持时，会选择优先级高的那个。

EXCLUDE\_ENGINES提供了排除某个引擎的功能，比如在一次训练过程中，为避免数据预处理图和主训练图抢占AiCore，可以给数据预处理图配置不使用AiCore引擎。

**参数取值：**

- “AiCore”：AI Core硬件加速引擎
- “AiVec”：Vector Core硬件加速引擎
- “AiCpu”：AI CPU硬件加速引擎

**配置示例：**

```c++
{ge::ir_option::EXCLUDE_ENGINES, "AiCore|AiVec"}
```

**产品支持情况：**

全量芯片支持。

## OP\_COMPILER\_CACHE\_MODE

用于配置算子编译磁盘缓存模式。此参数实际对应的options参数为`ge.op_compiler_cache_mode`。

**参数取值：**

- enable：（默认值）表示启用算子编译缓存。启用后可以避免针对相同编译参数及算子参数的算子重复编译，从而提升编译速度。
- force：启用算子编译缓存功能，区别于enable模式，force模式下会强制刷新缓存，即先删除已有缓存，再重新编译并加入缓存。比如当用户的python变更、依赖库变更、算子调优后知识库变更等，需要先指定为force用于先清理已有的缓存，后续再修改为enable模式，以避免每次编译时都强制刷新缓存。
- disable：表示禁用算子编译缓存，算子重新编译。

**配置示例：**

```c++
{ge::ir_option::OP_COMPILER_CACHE_MODE, "enable"}
```

**使用说明：**

- 如果要自行指定算子编译磁盘缓存的路径，则需要与OP\_COMPILER\_CACHE\_DIR配合使用。
- 启用算子编译缓存功能时，可以通过**配置文件**（算子编译完成后，会在OP\_COMPILER\_CACHE\_DIR参数指定路径下自动生成op\_cache.ini文件）、**环境变量**两种方式来设置缓存文件夹的磁盘空间大小：

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

- 由于force选项会先删除已有缓存，所以不建议在程序并行编译时设置，否则可能会导致其他模型因使用的缓存内容被清除而编译失败。
- 建议模型最终发布时设置编译缓存选项为disable或者force。
- 如果算子调优后知识库变更，则需要通过设置为force来刷新缓存，否则无法应用新的调优知识库，从而导致调优应用执行失败。
- 调试开关打开的场景：
  - OP\_DEBUG\_LEVEL配置非0值：会忽略OP\_COMPILER\_CACHE\_MODE参数的配置，不启用算子编译缓存功能，算子全部重新编译。
  - OP\_DEBUG\_CONFIG配置非空，且**未配置OP\_DEBUG\_LIST字段**，会忽略OP\_COMPILER\_CACHE\_MODE参数的配置，不启用算子编译缓存功能，算子全部重新编译。
  - OP\_DEBUG\_CONFIG配置非空，且**配置文件中配置了OP\_DEBUG\_LIST字段**：
    - 列表中的算子，忽略OP\_COMPILER\_CACHE\_MODE参数的配置继续重新编译。
    - 列表外的算子，如果OP\_COMPILER\_CACHE\_MODE参数配置为enable或force，则启用缓存功能；若配置为disable，则不启用缓存功能，仍旧重新编译。

**产品支持情况：**

全量芯片支持。

## OP\_COMPILER\_CACHE\_DIR

用于配置算子编译磁盘缓存的目录。此参数实际对应的options参数为`ge.op_compiler_cache_dir`。

**参数值格式**：路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符。

**默认值**：$HOME/atc\_data

**配置示例：**

```c++
{ge::ir_option::OP_COMPILER_CACHE_MODE, "enable"},
{ge::ir_option::OP_COMPILER_CACHE_DIR, "/home/test/data/atc_data"}
```

**使用约束：**

- 如果要自行指定算子编译磁盘缓存的路径，则需要与OP\_COMPILER\_CACHE\_MODE配合使用。
- 如果参数指定的路径存在且有效，则在指定的路径下自动创建子目录kernel\_cache；如果指定的路径不存在但路径有效，则先自动创建目录，然后在该路径下自动创建子目录kernel\_cache。
- 用户请不要在**默认缓存目录**下存放其他自有内容，自有内容在软件包安装或升级时会同默认缓存目录一并被删除。
- 通过该参数指定的**非默认缓存目录**无法删除（软件包安装或升级时不会被删除）。
- 算子编译磁盘缓存路径，除OP\_COMPILER\_CACHE\_DIR参数设置的方式外，还可以配置环境变量ASCEND\_CACHE\_PATH，几种方式优先级为：配置参数“OP\_COMPILER\_CACHE\_DIR”\>环境变量ASCEND\_CACHE\_PATH\>默认存储路径。关于环境变量ASCEND\_CACHE\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

**产品支持情况：**

全量芯片支持。

## OPTIMIZATION\_SWITCH

算子编译时，融合规则（Pass）的控制开关，该参数适用于所有融合规则。此参数实际对应的options参数为`ge.optimizationSwitch`。

**参数取值：**"Passname1:on;Passname2:off"，可以拼接多个key-value键值对，key为Pass名称，value为on（表示开）或off（表示关），不支持大小写模式匹配，多组配置使用英文分号分隔。可配置的融合规则请参见[融合规则列表](../../../../../user_guides/graph_dev/appendix/fusion_rule_list.md)。

**配置示例：**

```c++
{ge::ir_option::OPTIMIZATION_SWITCH, "Passname1:on;Passname2:off"}
```

**产品支持情况：**

全量芯片支持。
