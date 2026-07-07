# patterns

## 产品支持情况

全量芯片支持。

## 功能说明

定义需要匹配的模式列表，为legacy显式构图入口，适合直接返回一个或多个Pattern/Graph对象。

## 函数原型

```python
patterns(self) -> Iterable[PatternOrGraph]
```

## 参数说明

无

## 返回值说明

返回一个可迭代对象，其中每个元素为Pattern或Graph类型，表示需要匹配的子图模式。

## 约束说明

- 不能与@pattern方法同时使用。
- 不支持patterns\(self, inputs\)写法。
