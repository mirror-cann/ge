# GEGetErrorMsgV3

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

获取并清空与本接口在同一个进程或线程中的其它接口调用失败时的错误描述信息。

建议在执行报错时，调用该接口，获取错误信息以辅助定位问题。

## 函数原型

```c++
ge::AscendString GEGetErrorMsgV3()
```

## 参数说明

无

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | ge::AscendString | 执行过程中的报错信息。 |

## 约束说明

无
