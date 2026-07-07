# AnyTypeOperator

## 产品支持情况

全量芯片支持。

## 功能说明

任意类型操作符类，继承自Operator， 提供了对Operator类中IR相关方法的访问，包括动态输入/输出注册、输入/输出注册、属性注册和子图注册等功能。

## 构造函数原型

```c++
AnyTypeOperator(const char_t *name, const char_t *type)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| name | 输入 | 算子名称。 |
| type | 输入 | 算子类型。 |

## 返回值说明

AnyTypeOperator构造函数返回AnyTypeOperator类型的对象。

## 约束说明

无
