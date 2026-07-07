# save\_to\_air

## 产品支持情况

全量芯片支持。

## 功能说明

将图保存为air格式。

## 函数原型

```python
save_to_air(file_path: str) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| file_path | 输入 | 保存air文件的路径和文件名，string类型。 |

## 返回值说明

无

## 约束说明

- 如果file\_path不是字符串，抛出TypeError。
- 如果保存失败，抛出RuntimeError。
