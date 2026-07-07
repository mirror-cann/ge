# get\_data

## 产品支持情况

全量芯片支持。

## 功能说明

获取张量数据。

## 函数原型

```python
get_data() -> TensorLike
```

## 参数说明

无

## 返回值说明

[TensorLike类型](#tensorlike类型)张量数据。

## 约束说明

如果获取失败，抛出RuntimeError。

## TensorLike类型

TensorLike是一个类型别名，表示类似张量的对象：

- 单个数字（int或float）
- 数字列表（嵌套列表）

定义如下：

```python
TensorLike = Union[int, float, List['TensorLike']]
```
