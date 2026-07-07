# malloc

## 产品支持情况

全量芯片支持。

## 功能说明

在用户内存池中根据指定size大小申请device内存。

## 函数原型

```python
malloc(size: int) -> MemBlock
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| size | 输入 | 指定需要申请内存大小。 |

## 返回值说明

\([MemBlock](../MemBlock/MemBlock.md)\)内存Block对象。

## 约束说明

抽象方法用户必须实现。
