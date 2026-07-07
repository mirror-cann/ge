# AddControlEdge

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/es\_tensor\_holder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

控制边连接函数。

## 函数原型

```c++
Status AddControlEdge(const std::vector<EsTensorHolder> &ctrl_ins) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| ctrl_ins | 输入 | 控制边输入，支持多个。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无

## 调用示例

```c++
  auto builder = std::make_unique<ge::EsGraphBuilder>("graph");
  auto tensor0 = builder->CreateInput(0);
  auto tensor1 = builder->CreateInput(1);
  auto tensor2 = builder->CreateInput(2);
  std::vector<EsTensorHolder> ctrl_ins = {tensor1, tensor2};
  （void）tensor0.AddControlEdge(ctrl_ins); // tensor0的生产节点使用tensor1和tensor2的生产节点作为控制输入
```
