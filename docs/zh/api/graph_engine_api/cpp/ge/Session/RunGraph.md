# RunGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

同步运行指定ID对应的Graph图，输出运行结果。

该接口包括了编译，加载和运行Graph的操作。

## 函数原型

```c++
Status RunGraph(uint32_t graph_id, const std::vector<Tensor>& inputs, std::vector<Tensor>& outputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要运行图对应的ID。 |
| inputs | 输入 | 计算图输入Tensor，为Host上分配的内存空间。<br>使用std::vector\<Tensor>类型的inputs作为输入，用户输入进入数据队列时，需要对每个输入做一次内存的申请和拷贝，输入较大或者较多时可能存在性能瓶颈。 |
| outputs | 输出 | 计算图输出Tensor，用户无需分配内存空间，执行完成后GE会分配内存并赋值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：运行成功。<br>  - FAILED：运行失败。 |

## 约束说明

inputs与图中的data节点相对应，data节点的index属性表征inputs列表中对应数据的位置。即用户需要保证，可以按照data节点的index属性从inputs中获取对应的数据，否则返回错误。如果图中没有data节点，也可以输入的inputs为空。输出的outputs与用户指定的输出节点及输出端口个数与顺序相一致。
