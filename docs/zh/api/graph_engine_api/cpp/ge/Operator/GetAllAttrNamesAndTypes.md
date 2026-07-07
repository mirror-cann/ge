# GetAllAttrNamesAndTypes

## 产品支持情况

全量芯片支持。

## 头文件和库文件

- 头文件：\#include <graph/operator.h\>
- 库文件：libgraph.so

## 功能说明

获取该算子所有的已经设置的属性名称和属性类型，包含IR原型定义的普通属性和用户自定义属性。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
const std::map<std::string, std::string> GetAllAttrNamesAndTypes() const
graphStatus GetAllAttrNamesAndTypes(std::map<AscendString, AscendString> &attr_name_types) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| attr_name_types | 输出 | 所有的属性名称和属性类型。 |

## 返回值说明

graphStatus类型：GRAPH\_SUCCESS，代表成功；GRAPH\_FAILED，代表失败。

## 异常处理

无。

## 约束说明

无。
