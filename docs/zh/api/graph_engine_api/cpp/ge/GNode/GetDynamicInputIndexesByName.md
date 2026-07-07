# GetDynamicInputIndexesByName

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/gnode.h\>
- 库文件：libgraph.so

## 功能说明

通过算子的动态输入端口名获取动态输入端口indexes。

## 函数原型

```c++
graphStatus GetDynamicInputIndexesByName(const AscendString &name, std::vector<int32_t> &indexes)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 动态输入端口名。 |
| indexes | 输出 | 算子的动态输入端口号。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

无
