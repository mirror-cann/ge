# CreateFromIfNotEqual

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/compliant\_node\_builder.h\>
- 库文件：libeager\_style\_graph\_builder\_base.so、libeager\_style\_graph\_builder\_base\_static.a

## 功能说明

如果值不等于默认值，则创建属性值对象。

## 函数原型

```c++
template <typename T>
AttrValue CreateFromIfNotEqual(T &&value, typename std::decay<T>::type default_value)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| value | 输入 | 实际值。 |
| default_value | 输入 | 默认值。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | AttrValue | 创建的属性值对象。 |

## 约束说明

无
