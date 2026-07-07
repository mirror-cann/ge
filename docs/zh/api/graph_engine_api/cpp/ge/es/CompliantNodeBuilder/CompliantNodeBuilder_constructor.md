# CompliantNodeBuilder构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

CompliantNodeBuilder构造函数和析构函数。

## 函数原型

```c++
explicit CompliantNodeBuilder(ge::Graph *graph)
~CompliantNodeBuilder()
CompliantNodeBuilder(CompliantNodeBuilder &&) noexcept
CompliantNodeBuilder(const CompliantNodeBuilder &)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| graph | 输入 | 所属的图对象。 |

## 返回值说明

CompliantNodeBuilder构造函数返回CompliantNodeBuilder类型的对象。

## 约束说明

无
