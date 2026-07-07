# GetEventNum

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取整个编译图中已使用的Event数目。

## 函数原型

```c++
Status GetEventNum(size_t &num) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| num | 输出 | 已使用的Event数目。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | - SUCCESS：接口调用成功。<br>  - FAILED：接口调用失败。 |

## 约束说明

无
