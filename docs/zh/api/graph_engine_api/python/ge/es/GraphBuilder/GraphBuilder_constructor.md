# GraphBuilder构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

初始化一个GraphBuilder。

生命周期说明：

- 所有由该构建器创建的TensorHolder对象都保持对它的引用。
- 只要其任何张量仍被引用，构建器就不会被垃圾回收。
- 调用build\_and\_reset\(\)后，构建器进入已构建状态，不能再用于创建新张量。
- 需要创建新的GraphBuilder来构建另一个图。

## 函数原型

```python
GraphBuilder(name: Optional[str] = None)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| name | 输入 | 图名称，string类型，如果为None则默认为"graph"。 |

## 返回值说明

无

## 约束说明

- 如果name不是字符串或None，抛出TypeError。
- 如果图构建器创建失败，抛出RuntimeError。
