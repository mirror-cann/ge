# aclgrphGenerateForOp

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

根据单算子信息或单算子JSON文件构建Graph。

## 函数原型

```c++
graphStatus aclgrphGenerateForOp(const AscendString &op_type, const std::vector<TensorDesc> &inputs, const std::vector<TensorDesc> &outputs, Graph &graph)
graphStatus aclgrphGenerateForOp(const AscendString &json_path, std::vector<Graph> &graphs)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| op_type | 输入 | 算子类型。 |
| inputs | 输入 | 算子的输入Tensor格式列表。 |
| outputs | 输入 | 算子的输出Tensor格式列表。 |
| json_path | 输入 | 单算子的JSON文件路径。 |
| graph | 输出 | 生成的Graph。 |
| graphs | 输出 | 生成的Graph列表。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
