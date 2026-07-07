# aclgrphBuildFinalize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

系统完成模型构建后，通过该接口释放资源。

## 函数原型

```c++
void aclgrphBuildFinalize()
```

## 参数说明

无

## 返回值说明

无

## 约束说明

该接口在[aclgrphBuildInitialize](aclgrphBuildInitialize.md)之后调用，且仅能在进程退出时调用一次。
