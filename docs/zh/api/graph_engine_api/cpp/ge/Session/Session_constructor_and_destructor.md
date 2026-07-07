# Session构造函数和析构函数

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

Session构造函数和析构函数。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
explicit Session(const std::map<std::string, std::string> &options)
explicit Session(const std::map<AscendString, AscendString> &options)
~Session()
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| options | 输入 | map表，key为参数类型，value为参数值，均为字符串格式，描述Session的配置信息。<br>一般情况下可不填，与GEInitialize传入的全局options保持一致。<br>如需单独配置当前Session的配置信息时，可以通过此参数配置，支持的配置项请参见[options参数说明](../options_params/options_parameters_description.md)中session级别的参数。 |

## 返回值说明

无

## 约束说明

Session暂不支持并发执行，同时Session会独占资源，多个Session同时创建时，Session可能因为资源不足而创建失败。
