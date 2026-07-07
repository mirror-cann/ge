# EnableIrAttrMatch

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher\_config.h\>
- 库文件：libge\_compiler.so

## 功能说明

用于配置PatternMatcher的IR属性及其值匹配能力。

调用该函数后，PatternMatcher在图匹配过程中会对pattern中节点上携带的IR（Intermediate Representation）属性进行严格比对，包括属性名称数量、属性名称列表和属性值的匹配。

## 函数原型

```c++
PatternMatcherConfigBuilder &EnableIrAttrMatch()
```

## 参数说明

无

## 返回值说明

PatternMatcherConfigBuilder &引用，可进行成员函数的级联调用。

## 约束说明

匹配方式为二进制匹配，这是一种较为严格的匹配方式。若不符合预期，请关闭该开关，在Pass中自行处理值校验逻辑。
