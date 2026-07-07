# Shape构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

创建一个新的Shape对象。

## 函数原型

```python
Shape(dims: Optional[List[int]] = None)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| dims | 输入 | Shape的取值内容，Shape表征张量数据的维度大小，用List[int]表征每一个维度的具体大小。 |

## 返回值说明

Shape对象。

## 约束说明

如果dims不是整数列表，抛出TypeError。
