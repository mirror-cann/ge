# GEInitializeV2

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

初始化GE，完成运行准备。

## 函数原型

```c++
Status GEInitializeV2(const std::map<AscendString, AscendString> &options)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| options | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述初始化相关的配置信息，当前支持的配置信息请参考[options参数说明](../options_params/options_parameters_description.md)中全局级别的参数。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | 0：初始化成功。<br>其他：初始化失败。 |

## 约束说明

- GE不支持多实例运行，一次只能初始化一个。
- 多次调用该接口而没有调用[GEFinalizeV2](GEFinalizeV2.md)，运行不可预期。
