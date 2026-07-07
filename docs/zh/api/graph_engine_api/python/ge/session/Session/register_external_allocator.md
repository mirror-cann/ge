# register\_external\_allocator

## 功能说明

用户将自己的Allocator注册给GE，适用于使用用户的内存池场景。

## 函数原型

```python
register_external_allocator(stream: int, allocator: Allocator) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| stream | 输入 | 指定Allocator注册在哪个Stream上。 |
| allocator | 输入 | 用户Allocator对象，Allocator基于[Allocator](../../allocator/Allocator/Allocator.md)派生。 |

## 返回值说明

无

## 约束说明

- 如果stream不是整数，抛出TypeError。
- 如果allocator不是Allocator对象，抛出TypeError。
- 如果注册失败，抛出RuntimeError。
- 对于同一条流，多次调用本接口，以最后一次注册为准。
- 对于不同流，如果用户使用同一个Allocator，不可以多条流并发执行，在执行下一条Stream前，需要对上一Stream做流同步。
- 将Allocator中的内存释放给操作系统前，需要先调用“[acl.rt.synchronize\_stream](https://hiascend.com/document/redirect/CannCommunitysynchronize)”接口执行流同步，确保Stream中的任务已执行完成。
