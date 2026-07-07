# 内存管理

## ge.exec.atomicCleanPolicy

是否集中清理网络中所有memset算子（含有memset属性的算子都是memset算子）占用的内存。

**参数取值**：

- 0：（默认值）集中清理。
- 1：单独清理，对网络中每一个memset算子进行单独清理。当网络中memset算子内存过大时建议使用此种清理方式，对降低使用内存有明显效果，但可能会导致一定的性能损耗。

**配置示例：**

```c++
{"ge.exec.atomicCleanPolicy", "0"};
```

**必选/可选**：可选

**生效级别**：session

## ge.exec.disableReuseMemory

内存复用开关。

**参数取值：**

- 1：关闭内存复用。
- 0：（默认值）开启内存复用。

**配置示例：**

```c++
{"ge.exec.disableReuseMemory", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.exec.input\_fusion\_size

Host侧输入数据搬运到Device侧时，将用户离散多个输入数据合并拷贝的阈值，最小值为0 Byte，最大值为33554432 Byte（32MB），默认值为131072Byte（128KB）。若：

- 输入数据大小**<=**阈值，则合并输入，然后从host搬运到device。
- 输入数据大小**\>**阈值，或者阈值=0（功能关闭），则不合并，直接从host搬运到device。

例如用户有10个输入，有2个输入数据大小为100KB，2个输入数据大小为50KB，其余输入大于100KB，若设置：

- ge.exec.input\_fusion\_size=100KB，则上述4个输入合并为300KB，执行搬运；其他6个输入，直接从host搬运到device。
- ge.exec.input\_fusion\_size=0KB，则该功能关闭，无输入合并，即10个输入直接从host搬运到device。

该参数只在异步静态shape图执行时生效，即使用[RunGraphAsync](../Session/RunGraphAsync.md)接口运行Graph。

**配置示例：**

```c++
{"ge.exec.input_fusion_size", "131072"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.exec.inputReuseMemIndexes

用于配置是否开启图的输入节点的内存复用功能，开启后，输入节点的内存可作为模型执行过程中所需要的中间内存再次使用，从而达到降低内存峰值的目的。

参数取值为输入节点的index；如果对多个输入节点都开启内存复用，多个index间使用英文逗号分隔。输入节点需要设置属性index，说明是第几个输入，index从0开始。

注意：

- 若开启了输入节点的内存复用功能，需要用户确保输入的内存大小已经32字节对齐。
- 若所配置的输入index大于等于“输入个数”的值，属于非法index，该index配置不生效。
- 若开启了输入节点的内存复用功能，输入节点内存中的数据会被改写，图执行后，不可以继续使用输入节点内存中的内容。

**配置示例：**

```c++
{"ge.exec.inputReuseMemIndexes", "0,1,2"};
```

**必选/可选**：可选

**生效级别**：graph

## ge.exec.staticMemoryPolicy

网络运行时使用的内存分配方式。

**参数取值：**

- 0：（默认值）动态分配内存，即按照实际大小动态分配，不支持内存扩展。
- 2：静态shape内存动态扩展，训练与在线推理场景下，可以通过此取值实现同一Session中多张图之间的内存复用，即以最大图所需内存进行分配。例如，假设当前执行图所需内存超过前一张图的内存时，直接释放前一张图的内存，按照当前图所需内存重新分配。
- 3：仅动态shape支持内存动态扩展，解决内存动态分配时的碎片问题，降低动态shape网络内存占用。
- 4：静态shape和动态shape同时支持内存动态扩展。

> [!NOTE]说明
>
>- 多张图并发执行时，不支持配置为“2”和“4”。
>- 为兼容历史版本配置，配置为“1”的场景下，系统会按照“2”动态扩展内存的方式进行处理。
>- 配置为“3”和“4”的场景下，会有内存收益，但可能会造成一定的性能损失。

**配置示例：**

```c++
{"ge.exec.staticMemoryPolicy", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.exec.outputReuseInputMemIndexes

模型推理时，显式声明指定的输出张量与输入张量共享同一内存地址。

当用户在图编译前能明确内存复用关系时，可以通过该参数告知GE图编译器，GE将在编译阶段进行相应优化。典型场景如下：

Data\>TensorMove\>InplaceOp（原地算子）\>NetOutput，其中Data和NetOutput分别为模型输入和输出。若Data与NetOutput的生命周期满足复用条件，编译器将自动移除中间的TensorMove（内存拷贝算子），从而避免不必要的内存拷贝操作，提升模型运行时的性能。

**参数取值：**

"input\_index\_0,output\_index\_0|input\_index\_1,output\_index\_1|..."

- 每对input\_index\_X,output\_index\_Y表示**输入索引为X**的输入张量与**输出索引为Y**的输出张量内存地址相同。
- 多对索引之间用竖线"|"分隔。

**配置示例：**

假如模型有10个输入、10 个输出，若配置为：

```c++
ge.exec.outputReuseInputMemIndexes = "0,1|6,8"
```

则表示输出索引1与输入索引0的内存地址相同，输出索引8与输入索引6的内存地址相同。

说明：

- 索引有效性：声明的输入/输出索引必须属于模型定义的合法范围。
- 地址一致性：模型执行时，必须确保对应的输入输出传入相同的内存地址，否则将导致运行错误。

**必选/可选**：可选

**生效级别**：graph

## ge.exec.outputReuseMemIndexes

用于配置是否开启整图输出的内存复用功能，开启后，整图输出的内存可作为模型执行过程中所需要的中间内存再次使用，从而达到降低内存峰值的目的。

如果开启，配置为整图输出的index；如果对多个输出都开启内存复用，多个index间使用英文逗号分隔。

注意：

- 若开启了整图输出的内存复用功能，需要用户确保输出的内存大小已经32字节对齐。
- 输出的index，按照整图输出的顺序标识，index从0开始。
- 若所配置的输出index大于等于“输出个数”的值，属于非法index，该index配置不生效。

**配置示例：**

```c++
{"ge.exec.outputReuseMemIndexes", "0,1,2"};
```

**必选/可选**：可选

**生效级别**：graph

## ge.externalWeight

同一个Session内同时加载多个模型时，如果多个模型间的权重能够复用，建议通过此配置项将网络中Const/Constant节点的权重外置，实现多个模型间的权重复用，以减少权重的内存占用。

**参数取值：**

- 0：（默认值）权重不外置，直接保存在图中。
- 1：权重外置但不归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型；不同节点权重保存到不同的文件中，以weight\_<hash值\>命名。
- 2：权重外置且归一，将网络中所有Const/Constant节点的权重文件落盘，并将节点类型转换为FileConstant类型，所有权重保存到同一个文件中，以“原始根图名称\_weight\_combined”命名。

落盘路径说明：

- 若配置了ge.externalWeightDir，则权重文件会落盘到指定目录。
- 若环境中未配置环境变量ASCEND\_WORK\_PATH，则权重文件落盘至当前执行目录“tmp\_weight\__<pid\>_\__<sessionid__\>_”下。
- 若环境中配置环境变量ASCEND\_WORK\_PATH，则权重文件会落盘至$\{ASCEND\_WORK\_PATH\}/tmp\_weight_\_<pid\>_\__<sessionid__\>_目录下，关于ASCEND\_WORK\_PATH的详细说明，可参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

落盘路径优先级：ge.externalWeightDir \> $\{ASCEND\_WORK\_PATH\}/tmp\_weight_\_<pid\>_\__<sessionid__\>_  \>当前执行目录tmp\_weight\__<pid\>_\__<sessionid__\>_。

模型卸载时，会将tmp\_weight\__<pid\>_\__<sessionid__\>_目录删除。

**配置示例：**

```c++
{"ge.externalWeight", "1"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.externalWeightDir

用于配置保存外置权重文件落盘的路径。

**使用约束：**

- 如果要自行指定外置权重文件存放路径，需与ge.externalWeight参数配合使用。
- 优先级：ge.externalWeightDir \> $\{ASCEND\_WORK\_PATH\}/tmp\_weight_\_<pid\>_\__<sessionid__\>_  \>当前执行目录tmp\_weight\__<pid\>_\__<sessionid__\>_。

**配置示例：**

```c++
{"ge.externalWeight", "1"};
{"ge.externalWeightDir", "$HOME/your_tmp_path"};
```

**必选/可选**：可选

**生效级别**：全局/session

## ge.inputBatchCpy

Host侧输入数据搬运到Device侧时，是否开启批量内存拷贝功能。

该参数可以提升Host到Device的数据搬运性能，适用于需要频繁搬运数据且PCIE带宽使用率低的场景；开启该参数批量拷贝功能后，可以提升带宽利用率。

**参数取值：**

- 1：开启批量内存拷贝功能。该取值仅在用户输入个数大于1时，功能才会生效。
- 0：（默认值）关闭批量内存拷贝功能。

**使用约束**：

- 该参数仅支持如下产品：

    Ascend 950PR/Ascend 950DT

    Atlas A3 训练系列产品/Atlas A3 推理系列产品

    Atlas A2 训练系列产品/Atlas A2 推理系列产品

- 该特性通常在多Session场景下使用，考虑到不同Session中的输入个数可能不同，因此推荐配置为session级，根据输入情况按需启用，不建议配置为全局级别或者graph级别。
- Session初始化时传入该参数，后续运行Graph时，目前只能通过[RunGraphAsync](../Session/RunGraphAsync.md)接口启用该特性。
- 若网络初始输入个数为1，即使配置了批量拷贝功能也不会生效。
- 若同时配置ge.exec.input\_fusion\_size合并拷贝和开启ge.inputBatchCpy批量拷贝功能，合并拷贝的阈值可能会影响批量拷贝功能。

    例如，用户有5个输入，有4个输入满足合并拷贝阈值，此4个输入执行合并拷贝功能，剩余的1个输入不会执行批量拷贝功能。

**配置示例：**

```c++
{"ge.inputBatchCpy", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph

## ge.featureBaseRefreshable

配置feature内存地址是否可刷新。若用户需要自行管理feature内存并需要多次刷新该地址，则可将该参数配置为可刷新。

该参数仅限于静态shape图。

**参数取值：**

0：（默认值）feature内存地址不可刷新。

1：支持刷新模型的feature内存地址。

**配置示例：**

```c++
{"ge.featureBaseRefreshable", "0"};
```

**必选/可选**：可选

**生效级别**：全局/session/graph
