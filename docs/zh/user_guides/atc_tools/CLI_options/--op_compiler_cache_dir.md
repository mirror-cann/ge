# --op\_compiler\_cache\_dir

## 产品支持情况

全量芯片支持

## 功能说明

用于配置算子编译磁盘缓存的路径。

## 关联参数

如果要自行指定算子编译磁盘缓存的路径，需--op\_compiler\_cache\_dir与[--op\_compiler\_cache\_mode](--op_compiler_cache_mode.md)参数配合使用。

## 参数取值

**参数值：**
存放算子编译磁盘缓存的路径。

**参数值格式：**
路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**

- 如果[--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)参数指定的路径存在且有效，则在指定的路径下自动创建子目录kernel\_cache；如果指定的路径不存在但路径有效，则先自动创建目录，然后在该路径下自动创建子目录kernel\_cache。
- 用户请不要在**默认缓存目录**下存放其他自有内容，自有内容在软件包安装或升级时会同默认缓存目录一并被删除。
- 通过该参数指定的**非默认缓存目录**无法删除（软件包安装或升级时不会被删除）。

**参数默认值：**$HOME/atc\_data

## 推荐配置及收益

无。

## 示例

```bash
atc --op_compiler_cache_dir=$HOME/atc_data --op_compiler_cache_mode=enable ...
```

## 使用约束

算子编译磁盘缓存路径，除[--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)参数设置的方式外，还可以配置环境变量ASCEND\_CACHE\_PATH，几种方式优先级为

配置参数“[--op\_compiler\_cache\_dir](--op_compiler_cache_dir.md)”\>环境变量ASCEND\_CACHE\_PATH\>默认存储路径。

关于环境变量ASCEND\_CACHE\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。
