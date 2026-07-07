# GeSession构造函数和析构函数

## 头文件/库文件

- 头文件：\#include <ge/ge\_api\_v2.h\>
- 库文件：libge\_runner\_v2.so

## 功能说明

GeSession构造函数和析构函数。

## 函数原型

```c++
explicit GeSession(const std::map<AscendString, AscendString> &options)
~GeSession()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| options | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述GeSession的配置信息。<br>一般情况下可不填，与GEInitializeV2传入的全局options保持一致。<br>如需单独配置当前GeSession的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../options_params/options_parameters_description.md)中session级别的参数。 |

## 返回值说明

无

## 约束说明

GeSession暂不支持并发执行，同时GeSession会独占资源，多个GeSession同时创建时，GeSession可能因为资源不足而创建失败。
