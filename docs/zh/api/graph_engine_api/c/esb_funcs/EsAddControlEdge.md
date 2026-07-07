# EsAddControlEdge

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/esb\_funcs.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

控制边连接函数。

## 函数原型

```c
uint32_t EsAddControlEdge(EsCTensorHolder *dest_ctrl_tensor, EsCTensorHolder **src_ctrl_tensors,int64_t ctrl_tensors_num)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| dest_ctrl_tensor | 输入 | 控制边连边对象。 |
| src_ctrl_tensors | 输入 | 控制边输入。 |
| ctrl_tensors_num | 输入 | 控制边数量。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | uint32_t | 成功为0，其他失败。 |

## 约束说明

无

## 调用示例

```c
auto tensor0 = _builder->CreateInput(0).GetCTensorHolder();
auto tensor1 = _builder->CreateInput(1).GetCTensorHolder();
auto tensor2 = _builder->CreateInput(2).GetCTensorHolder();
std::vector<EsCTensorHolder *> ctrl_ins = {tensor1, tensor2};
EsAddControlEdge(tensor0, ctrl_ins.data(), 2)；
```
