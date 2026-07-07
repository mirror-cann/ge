# SaveGraphsToPb

## 产品支持情况

- Ascend 950PR/Ascend 950DT：不支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：不支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 200I/500 A2 推理产品：不支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持
- MC62CM12A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- IPV350：不支持

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

**该接口已废弃，请勿使用。**

保存Session内图接口，将图保存为pb文件，支持Session内的权重共享功能（即Session内两张图若有两个权重值是一致的，则只会保存一份权重）。

**该接口与[ShardGraphsToFile](ShardGraphsToFile.md)接口区别：**

该接口适用于任何图，而[ShardGraphsToFile](ShardGraphsToFile.md)适用于大模型分布式编译切分场景。

## 函数原型

```c++
Status SaveGraphsToPb(const char_t *file_path) const
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| file_path | 输入 | 保存图和权重的目录，必须为一个合法路径。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>FAILED：失败 |

## 约束说明

同一张图内部的权重不可共享。
