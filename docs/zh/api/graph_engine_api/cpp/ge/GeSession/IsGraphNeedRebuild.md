# IsGraphNeedRebuild

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

Graph是否需要重新编译。

## 函数原型

```c++
bool IsGraphNeedRebuild(uint32_t graph_id)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph_id | 输入 | Graph对应的ID。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | bool | - False：图可以正常调用[RunGraphAsync](./RunGraphAsync.md)。<br>  - True：需要[RemoveGraph](./RemoveGraph.md)后，重新[AddGraph](./AddGraph.md)和[CompileGraph](./CompileGraph.md)。 |

## 约束说明

无
