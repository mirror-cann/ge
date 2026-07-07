# BuildGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

同步编译指定ID对应的Graph图，生成可用于执行的模型。

## 函数原型

```c++
Status BuildGraph(uint32_t graph_id, const std::vector<InputTensorInfo> &inputs)
Status BuildGraph(uint32_t graph_id, const std::vector<ge::Tensor> &inputs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |
| inputs | 输入 | 当前图对应的输入数据。<br>InputTensorInfo结构体定义请参见[InputTensorInfo](../InputTensorInfo.md)。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：图编译成功。<br>FAILED：图编译失败。 |

## 约束说明

无
