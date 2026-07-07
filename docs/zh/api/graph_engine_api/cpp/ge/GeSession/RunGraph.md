# RunGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

同步运行指定ID对应的Graph图，输出运行结果。

本接口与[RunGraphAsync](./RunGraph.md)/[RunGraphWithStreamAsync](./RunGraphWithStreamAsync.md)互斥；若在调用本接口前未执行[LoadGraph](./LoadGraph.md)完成图加载，则本接口将自动调用LoadGraph以完成加载。

## 函数原型

```c++
Status RunGraph(uint32_t graph_id, const std::vector<gert::Tensor>& inputs, std::vector<gert::Tensor>& outputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要运行图对应的ID。 |
| inputs | 输入 | 计算图输入Tensor，可以位于Host，也可以位于Device上。<br>如果数据位于Host上，而执行在device上，用户输入进入数据队列时，需要对每个输入做一次内存的申请和拷贝，输入较大或者较多时可能存在性能瓶颈。 |
| outputs | 输出 | 计算图输出Tensor，用户无需分配内存空间，执行完成后GE会分配Host内存并赋值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：图运行成功。<br>  - FAILED：图运行失败。 |

## 约束说明

- inputs与图中的data节点相对应，data节点的index属性表征inputs列表中对应数据的位置。即用户需要保证，可以按照data节点的index属性从inputs中获取对应的数据，否则返回错误。如果图中没有data节点，也可以输入的inputs为空。输出的outputs与用户指定的输出节点及输出端口个数与顺序相一致。
- 传入的Tensor数据如果位于Device上，则Tensor在Device侧的存储地址，必须32字节对齐，否则可能会出现未定义错误。
