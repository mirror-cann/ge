# PatternFusionPass构造函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pass/pattern\_fusion\_pass.h\>
- 库文件：libge\_compiler.so

## 功能说明

PatternFusionPass的构造函数。

通过该构造函数创建的Pass，其Match Config各项均默认为关闭状态；子类可传入PatternMatcherConfig以开启所需的匹配选项。

## 函数原型

```c++
PatternFusionPass()
explicit PatternFusionPass(std::unique_ptr<PatternMatcherConfig> match_config)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| std::unique_ptr\<PatternMatcherConfig> match_config | 输入 | 匹配规则配置。 |

## 返回值说明

无

## 约束说明

无
