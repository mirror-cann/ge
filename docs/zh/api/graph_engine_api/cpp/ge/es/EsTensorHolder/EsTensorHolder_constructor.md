# EsTensorHolder构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

EsTensorHolder构造函数。

## 函数原型

```c++
EsTensorHolder(EsCTensorHolder *tensor) : tensor_holder_(tensor) {}
EsTensorHolder() = default
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| tensor | 输入 | EsCTensorHolder指针，指向底层C实现对象。 |

## 返回值说明

EsTensorHolder构造函数返回EsTensorHolder类型的对象。

## 约束说明

无
