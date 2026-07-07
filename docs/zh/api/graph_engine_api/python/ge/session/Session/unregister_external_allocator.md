# unregister\_external\_allocator

## 功能说明

将用户基于Stream注册的Allocator销毁，适用于使用用户的内存池场景。

## 函数原型

```python
unregister_external_allocator(stream: int) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| stream | 输入 | 图ID。 |

## 返回值说明

无

## 约束说明

- 如果stream不是整数，抛出TypeError。
- 如果销毁失败，抛出RuntimeError。
- 更多约束请参见[约束说明](../../../../cpp/ge/Session/UnregisterExternalAllocator.md#约束说明)。
