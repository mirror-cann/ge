# aclgrphGetIRVersion

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_ir\_build.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取模型构建相关接口的版本号。

## 函数原型

```c++
graphStatus aclgrphGetIRVersion(int32_t *major_version, int32_t *minor_version, int32_t *patch_version)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| major_version | 输出 | 主版本号。 |
| minor_version | 输出 | 中间版本号。 |
| patch_version | 输出 | bug修复版本号。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功。<br>其他值：失败。 |

## 约束说明

入参建议采用整型引用的方式调用接口，如传入整型空指针，则会验空返回失败。
