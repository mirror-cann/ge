# UpdateInputDesc

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

更新指定输入端口的tensor格式。

## 函数原型

```c++
graphStatus UpdateInputDesc(const int32_t index, const TensorDesc &tensor_desc)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| index | 输入 | 指定更新的输入端口。 |
| tensor_desc | 输入 | 需要更新的tensor格式。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
