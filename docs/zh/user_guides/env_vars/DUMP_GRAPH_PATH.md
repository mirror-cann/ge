# DUMP\_GRAPH\_PATH

## 功能描述

指定DUMP图文件的保存路径，可配置为绝对路径或脚本执行目录的相对路径。

指定的路径支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、中划线（-）、句点（.）、中文字符，执行用户需具有读、写、执行权限。

- 如果指定的路径不存在，则会自动创建相应目录。
- 如果指定的路径执行用户权限不足，则会在当前执行目录下生成DUMP图文件。

> [!NOTE]说明
>
>1. 设置“DUMP\_GRAPH\_PATH”环境变量的场景下，程序会在指定路径下创建“pid\_$\{pid\}\_deviceid\_$\{deviceid\}”子文件夹，存储DUMP图文件。其中$\{pid\}为进程ID，$\{deviceid\}为设备ID。
>2. 若不设置“DUMP\_GRAPH\_PATH”环境变量，DUMP图文件直接存储在当前脚本执行路径，不会创建“pid\_$\{pid\}\_deviceid\_$\{deviceid\}”子文件夹。

DUMP图文件存储路径优先级为：

环境变量“ASCEND\_DUMP\_PATH”\> 环境变量“DUMP\_GRAPH\_PATH” \> 环境变量“ASCEND\_WORK\_PATH” \> 默认存储路径（当前脚本执行路径）。

## 配置示例

```bash
export DUMP_GRAPH_PATH=/home/dumpgraph
```

则DUMP图文件存储在/home/dumpgraph/pid\_$\{pid\}\_deviceid\_$\{deviceid\}路径下，例如/home/dumpgraph/pid\_53343\_deviceid\_0。

## 使用约束

此环境变量需要配合[DUMP\_GE\_GRAPH](DUMP_GE_GRAPH.md)使用，即开启[DUMP\_GE\_GRAPH](DUMP_GE_GRAPH.md)的场景下，可通过DUMP\_GRAPH\_PATH指定DUMP图文件的存储路径。

## 支持的型号

全量芯片支持
