# dump\_to\_stream

## 产品支持情况

全量芯片支持。

## 功能说明

将图导出到字符串流。

## 函数原型

```python
dump_to_stream(format: DumpFormat = DumpFormat.kReadable) -> str
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| format | 输入 | 导出格式，默认为kReadable。 |

## 返回值说明

图的字符串表示，string类型。

## 约束说明

- 如果format不是有效的DumpFormat，抛出TypeError。
- 如果导出失败，抛出RuntimeError。
