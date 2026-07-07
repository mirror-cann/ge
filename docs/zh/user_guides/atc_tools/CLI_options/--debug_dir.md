# --debug\_dir

## 产品支持情况

全量芯片支持

## 功能说明

用于配置模型转换过程中算子编译生成的调试相关过程文件的路径。

过程文件包括但不限于算子.o（算子二进制文件）、.json（算子描述文件）、.cce等文件，具体生成的文件以[--op\_debug\_level](--op_debug_level.md)参数设置的取值为准。

## 关联参数

如果要自行指定算子编译的过程文件存放路径，需--debug\_dir参数与[--op\_debug\_level](--op_debug_level.md)（取值为非0）参数配合使用。

## 参数取值

**参数值：**
算子编译生成的调试相关文件的路径。

**参数值格式：**
路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数值约束：**
如果使用该参数，则在执行atc命令之前，请先创建该参数要指定的目录，并且该目录需要有读写权限。

**参数默认值：**
在执行atc命令的当前路径./kernel\_meta文件夹中生成算子编译的过程文件。

## 推荐配置及收益

无。

## 示例

例如创建的目录名为debug\_info，则执行命令为：

```bash
atc --debug_dir=$HOME/module/out/debug_info --op_debug_level=1 ...
```

## 使用约束

算子编译生成的调试文件存储路径，除[--debug\_dir](--debug_dir.md)参数设置的方式外，还可以配置环境变量ASCEND\_WORK\_PATH，几种方式优先级为：配置参数“[--debug\_dir](--debug_dir.md)”\>环境变量ASCEND\_WORK\_PATH \>默认存储路径。

关于环境变量ASCEND\_WORK\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。
