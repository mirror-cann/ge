# RunGraphAsync

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

异步运行指定ID对应的Graph图，输出运行结果。

- 本接口与[RunGraph](./RunGraph.md)/[RunGraphWithStreamAsync](./RunGraphWithStreamAsync.md)互斥，若在调用本接口前未执行[LoadGraph](./LoadGraph.md)完成图加载，则本接口将自动调用LoadGraph以完成加载。
- 本接口与RunGraph均为执行指定id对应的图，并输出结果，区别于RunGraph的是，该接口：
  - 异步运行。
  - 用户通过回调函数RunAsyncCallbackV2获取图计算结果，即用户自行定义函数RunAsyncCallbackV2。该回调函数，当Status为SUCCESS时，即可处理数据。

## 函数原型

```c++
Status RunGraphAsync(uint32_t graph_id, const std::vector<gert::Tensor> &inputs, RunAsyncCallbackV2 callback)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| inputs | 输入 | 计算图输入Tensor，可以位于Host，也可以位于Device上。<br>如果数据位于Host上，而执行在device上，用户输入进入数据队列时，需要对每个输入做一次内存的申请和拷贝，输入较大或者较多时可能存在性能瓶颈。 |
| callback | 输入 | 当前图对应的回调函数，输出是GE申请的Host内存。回调函数详见表格下方的RunAsyncCallbackV2。 |

```c++
using Status = uint32_t;
using RunAsyncCallbackV2 = std::function<void(Status, std::vector<gert::Tensor> &)>;
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_GE_NOT_INITIALIZED：GE未初始化。<br>SUCCESS：异步运行图成功。<br>FAILED：异步运行图失败。 |

## 约束说明

传入的Tensor数据如果位于Device上，则Tensor在Device侧的存储地址，必须32字节对齐，否则可能会出现未定义错误。

## 调用示例

1. 调用接口AddGraph加载子图。

    ```c++
    std::map <AscendString, AscendString> options;
    ge::GeSession *session = new GeSession(options);

    uint32_t graph_id = 0;
    ge::Graph graph;
    Status ret = session->AddGraph(graph_id, graph);
    ```

2. 用户自定义RunAsyncCallbackV2，来决定如何处理数据，例如：

    ```c++
    void CallBack(Status result, std::vector<gert::Tensor> &out_tensor) {
    if(result == ge::SUCCESS) {
    // 读取out_tensor数据， 用户根据需求处理数据；
    for(auto &tensor : out_tensor) {
      auto data = tensor.GetAddr();
      int64_t length = tensor.GetSize();
      }
    }
    }
    ```

3. 定义好指定图的输入数据const std::vector\<gert::Tensor\> &inputs。
4. 调用接口RunGraphAsync\(graph\_id, inputs, CallBack\)。
