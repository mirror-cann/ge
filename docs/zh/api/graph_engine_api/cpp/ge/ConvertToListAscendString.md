# ConvertToListAscendString

## 产品支持情况

全量芯片支持。

## 头文件

\#include <graph/operator\_reg.h\>

## 功能说明

定义了一个模板函数ConvertToListAscendString，用于将不同类型的字符串列表转换为AscendString类型的列表。

## 函数原型

```c++
template<typename T> std::vector<ge::AscendString> ConvertToListAscendString(T strs)
```

支持以下两种拓展：

- template<\> inline std::vector\<ge::AscendString\> ConvertToListAscendString\(std::vector\<std::string\> strs\)

    对于std::vectorstd::string类型的字符串列表，先将其转换为std::vector\<const char \*\>类型，然后再进行转换。

- template<\> inline std::vector\<ge::AscendString\> ConvertToListAscendString\(std::vector\<ge::AscendString\> strs\)

    对于std::vector\<ge::AscendString\>类型的字符串列表，直接返回原列表。

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| strs | 输入 | 待转换的字符串列表。 |

## 返回值说明

转换后的AscendString类型字符串列表。

## 约束说明

无。
