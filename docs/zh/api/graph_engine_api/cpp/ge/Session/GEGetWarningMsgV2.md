# GEGetWarningMsgV2

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

获取并清空与本接口在同一个进程或线程中的其它接口调用失败时的告警信息。

程序执行结束需要查看告警信息时，调用该接口打印告警信息，属于ABI兼容接口（对应编译文件中的D\_GLIBCXX\_USE\_CXX11\_ABI），返回ge::AscendString。与[GEGetWarningMsg](GEGetWarningMsg.md)不能同时使用，推荐使用该接口。

## 函数原型

```c++
ge::AscendString GEGetWarningMsgV2()
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::AscendString | 执行过程中的告警信息。 |

## 约束说明

无
