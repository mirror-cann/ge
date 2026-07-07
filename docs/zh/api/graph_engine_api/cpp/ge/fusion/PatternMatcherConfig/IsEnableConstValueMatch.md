# IsEnableConstValueMatch

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/fusion/pattern\_matcher\_config.h\>
- 库文件：libge\_compiler.so

## 功能说明

返回是否开启Const值匹配能力。

## 函数原型

```c++
bool IsEnableConstValueMatch() const
```

## 参数说明

无

## 返回值说明

bool类型取值：

- 若返回true，pattern matcher在匹配过程中将对pattern中定义的const/constant上携带的Tensor进行值的匹配，值相等才认为匹配成功。
- 若返回false，节点上的Const不参与匹配。

## 约束说明

匹配方式为二进制匹配，其实一种较为严格的匹配方式（对于浮点类数据类型来说）。若不符合预期，请关闭该开关，在Pass中自行处理值校验逻辑。
