# GetAllIrAttrNamesAndTypes

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

获取该算子所有的IR原型定义的属性名称和属性类型，包含普通和必选属性两种。

## 函数原型

```c++
graphStatus GetAllIrAttrNamesAndTypes(std::map<AscendString, AscendString> &attr_name_types) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name_types | 输出 | 所有的IR原型定义的属性名称和属性类型。 |

## 返回值说明

graphStatus类型：GRAPH\_SUCCESS，代表成功；GRAPH\_FAILED，代表失败。

## 异常处理

无。

## 约束说明

无。
