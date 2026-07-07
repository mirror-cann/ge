# RemoveGraph

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

在当前GeSession中删除指定ID对应的Graph。

## 函数原型

```c++
Status RemoveGraph(uint32_t graph_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | 要删除的Graph所对应的ID。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | GE_CLI_SESS_REMOVE_FAILED：删除图时序列化转换失败。<br>SUCCESS：删除图成功。<br>FAILED：删除图失败。 |

## 约束说明

无
