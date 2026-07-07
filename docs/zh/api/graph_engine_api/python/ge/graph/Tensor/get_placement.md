# get\_placement

## 产品支持情况

全量芯片支持。

## 功能说明

获取数据所在存储位置。

## 函数原型

```python
get_placement() -> Placement
```

## 参数说明

无

## 返回值说明

\(Placement\)Tensor数据的存储位置。

## 约束说明

- 如果返回值不是有效的Placement，抛出ValueError。
