# --output

## 产品支持情况

全量芯片支持

## 功能说明

- 如果是开源框架的网络模型：

    存放转换后的离线模型的路径以及文件名，例如：*$HOME/module/out/tf\_resnet50*，转换后的模型文件名以指定的为准，自动以.om后缀结尾，例如：*tf\_resnet50.om*或*tf\_resnet50\_<os\>\_<arch\>.om*，若.om文件名中包含操作系统及架构，则该文件只能在该操作系统及架构的运行环境中使用。

- 如果是单算子描述文件（JSON格式）：

    存放转换后的单算子模型的路径，例如：*$HOME/singleop/out/op\_model*。转换后的模型文件命名规则默认为：序号\_算子类型\_输入的描述\(dataType\_format\_shape\)\_输出的描述\(dataType\_format\_shape\)，如果不采用默认命名规则，可以通过单算子描述文件中的name属性指定模型文件名。

## 关联参数

- 若使用atc命令转换出来的om离线模型文件名中含操作系统及架构，但操作系统及其架构与模型运行环境不一致时，则需要与[--host\_env\_os](--host_env_os.md)、[--host\_env\_cpu](--host_env_cpu.md)参数配合使用，设置模型运行环境的操作系统类型及架构。

## 参数取值

**参数值：**

- 如果是开源框架的网络模型：存放转换后的离线模型的路径以及文件名。
- 如果是单算子描述文件（JSON格式）：存放转换后的单算子模型的路径。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

## 推荐配置及收益

无。

## 示例

- Caffe框架网络模型：

    ```bash
    atc --output=$HOME/module/out/caffe_resnet50 ...
    ```

- TF框架网络模型：

    ```bash
    atc --output=$HOME/module/out/tf_resnet50 ...
    ```

- ONNX网络模型：

    ```bash
    atc --output=$HOME/module/out/onnx_resnet50 ...
    ```

- 单算子描述文件：

    ```bash
    atc --output=$HOME/singleop/out/op_model ...
    ```

## 依赖约束

无。
