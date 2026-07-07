# EnableConstValueMatch

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher\_config.h\>
- 库文件：libge\_compiler.so

## 功能说明

用于配置PatternMatcher的常量值匹配能力。

调用该函数后，PatternMatcher在图匹配过程中会对pattern中定义的const/constant节点上携带的Tensor进行值的严格比对，只有当实际图中的常量值与pattern定义中的常量值完全相等（二进制匹配）时，才认为匹配成功。

## 函数原型

```c++
PatternMatcherConfigBuilder &EnableConstValueMatch()
```

## 参数说明

无

## 返回值说明

PatternMatcherConfigBuilder &引用，可进行成员函数的级联调用。

## 约束说明

匹配方式为二进制匹配，其是一种较为严格的匹配方式。若不符合预期，请关闭该配置，在Pass中自行处理值校验逻辑。
