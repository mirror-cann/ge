# dump\_to\_file

## 产品支持情况

全量芯片支持。

## 功能说明

将图导出到文件。

## 函数原型

```python
dump_to_file(format: DumpFormat = DumpFormat.kReadable, suffix: str = "") -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| format | 输入 | 导出格式，默认为kReadable，详见[DumpFormat](../../../ge/DumpFormat.md)。 |
| suffix | 输入 | 文件名后缀，string类型，默认为空字符串。 |

## 返回值说明

无

## 约束说明

- 如果format不是有效的DumpFormat，抛出TypeError。
- 如果suffix不是字符串，抛出TypeError。
- 如果导出失败，抛出RuntimeError。
- 导出的pbtxt格式只包含图结构，不包含权重数据或其他属性。
