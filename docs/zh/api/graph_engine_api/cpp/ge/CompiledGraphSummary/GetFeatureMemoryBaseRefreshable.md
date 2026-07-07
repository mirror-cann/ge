# GetFeatureMemoryBaseRefreshable

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取图是否支持Feature内存基址刷新。

## 函数原型

```c++
Status GetFeatureMemoryBaseRefreshable (bool &v) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| v | 输出 | 内存基址是否可刷新。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败。 |

## 约束说明

无
