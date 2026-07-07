# RunGraphAsync

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

异步运行指定ID对应的Graph图，输出运行结果。

此函数与[RunGraph](RunGraph.md)均为执行指定id对应的图，并输出结果，区别于RunGraph的是，该接口：

- 异步运行。
- 承载数据的数据格式不一样，但含义相同。
- 用户通过回调函数RunAsyncCallback获取图计算结果，即用户自行定义函数RunAsyncCallback。该回调函数，当status为success时，即可处理数据。

## 函数原型

```c++
Status RunGraphAsync(uint32_t graph_id, const std::vector<ge::Tensor> &inputs, RunAsyncCallback callback)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要运行图对应的ID。 |
| inputs | 输入 | 计算图输入Tensor。<br>使用std::vector\<Tensor>类型的inputs作为输入，用户输入进入数据队列时，需要对每个输入做一次内存的申请和拷贝，输入较大或者较多时可能存在性能瓶颈。 |
| callback | 输入 | 当前图对应的回调函数。详情请参见表格下面的RunAsyncCallback内容。 |

```c++
using Status = uint32_t;
using RunAsyncCallback = std::function<void(Status, std::vector<ge::Tensor> &)>;
```

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_GE_NOT_INITIALIZED：GE未初始化。<br>SUCCESS：异步运行图成功。<br>FAILED：异步运行图失败。 |

## 约束说明

无

## 调用示例

1. 调用接口AddGraph加载子图。

    ```c++
    std::map <AscendString, AscendString> options;
    ge::GeSession *session = new GeSession(options);

    uint32_t graph_id = 0;
    ge::Graph graph;
    Status ret = session->AddGraph(graph_id, graph);
    ```

2. 用户自定义RunAsyncCallback，来决定如何处理数据，例如：

    ```c++
    void CallBack(Status result, std::vector<ge::Tensor> &out_tensor) {
    if(result == ge::SUCCESS) {
    // 读取out_tensor数据， 用户根据需求处理数据；
    for(auto &tensor : out_tensor) {
      auto data = tensor.GetData();
      int64_t length = tensor.GetSize();
      }
    }
    }
    ```

3. 定义好指定图的输入数据const std::vector\<ge::Tensor\> &inputs。
4. 调用接口RunGraphAsync\(graph\_id, inputs, CallBack\)。
