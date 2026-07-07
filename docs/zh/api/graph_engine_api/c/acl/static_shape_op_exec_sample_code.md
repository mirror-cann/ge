# 执行固定Shape算子示例代码

本节介绍基于单算子模型执行的方式调用固定Shape算子的关键接口、示例代码。

## 前提条件

在调用acl接口执行固定Shape算子前，需提前编译算子。此处借助ATC工具编译Add算子的模型文件为例：

1. 先构造该算子的描述文件（\*.json文件，描述输入输出Tensor描述、算子属性等）。

    Add算子的描述文件示例如下：

    ```bash
    [
      {
        "op": "Add",
        "input_desc": [
          {
            "format": "ND",
            "shape": [8, 16],
            "type": "int32"
          },
          {
            "format": "ND",
            "shape": [8, 16],
            "type": "int32"
          }
        ],
        "output_desc": [
          {
            "format": "ND",
            "shape": [8, 16],
            "type": "int32"
          }
        ]
      }
    ]
    ```

2. 借助ATC工具，将该算子描述文件编译成单算子模型文件（\*.om文件），再分别调用acl接口加载om模型文件、执行算子。

    ATC工具的命令示例如下：

    ```c++
    atc --singleop=$HOME/singleop/add.json --output=$HOME/singleop/out/op_model --soc_version=<soc_version>
    ```

    关键参数解释如下，详细参数取值及约束说明请参见[《ATC离线模型编译工具》](https://hiascend.com/document/redirect/cannCommunityATC)：

    - --singleop：单算子描述文件（json格式）的路径。
    - --output：存放单算子模型文件的目录。
    - --soc\_version：AI处理器的版本。_<soc\_version\>_请根据实际情况替换。

## 示例代码

以下是单算子加载、执行关键步骤的代码示例，不能直接拷贝编译运行，仅供参考。调用接口后，需增加异常处理的分支，并记录报错日志、提示日志，此处不一一列举。

您可以单击[aclop\_invocation](https://gitcode.com/cann/asc-devkit/tree/master/examples/02_features/00_framework_launch/aclop_invocation)获取样例。

```c++
// 1.初始化
aclRet = aclInit(nullptr);

// 2.运行时资源申请（使用默认Context、默认Stream，默认Stream在作为其它接口入参时，可传空指针）
aclRet = aclrtSetDevice(0);
// 获取软件栈的运行模式，不同运行模式影响后续的接口调用流程（例如是否进行数据传输等）
aclrtRunMode runMode;
bool g_isDevice = false;
aclError aclRet = aclrtGetRunMode(&runMode);
g_isDevice = (runMode == ACL_DEVICE);

// 3.加载单算子模型文件（*.om文件）
// 该目录相对可执行文件所在的目录，例如，编译出来的可执行文件存放在out目录下，此处就表示out/op_models目录
aclRet = aclopSetModelDir("op_models");

// 4.执行算子
// opType表示算子类型名称，例如Add
// numInputs表示算子输入个数，例如Add算子是2个输入
// inputDesc表示算子输入tensor描述的数组，描述每个输入的format、shape、数据类型
// inputs表示算子输入tensor数据
// numOutputs表示算子输出个数，例如Add算子是1个输出
// outputDesc表示算子输出tensor描述的数组，描述每个输出的format、shape、数据类型
// outputs表示算子输出tensor数据
// attr表示算子属性，如果算子没有属性，也需要调用aclopCreateAttr接口创建aclopAttr类型的数据
// stream用于维护一些异步操作的执行顺序

aclopExecuteV2(opType, numInputs, inputDesc, inputs,
               numOutputs, outputDesc, outputs, attr, nullptr);

// 处理执行算子后的输出数据，例如在屏幕上显示、写入文件等，由用户根据实际情况自行实现
// ......

// 阻塞应用运行，直到指定Stream中的所有任务都完成
aclrtSynchronizeStream(nullptr);

// 5. 释放运行时资源（默认Context、Stream无需用户释放，调用aclrtResetDevice接口后自动释放）
aclRet = aclrtResetDevice(0);

// 6.去初始化
aclRet = aclFinalize();

// ....
```
