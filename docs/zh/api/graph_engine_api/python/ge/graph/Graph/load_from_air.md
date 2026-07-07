# load\_from\_air

## 产品支持情况

全量芯片支持。

## 功能说明

从air格式加载图。

## 函数原型

```python
load_from_air(file_path: str) -> None
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| file_path | 输入 | 加载air文件的路径和文件名，string类型。 |

## 返回值说明

无

## 约束说明

- 如果file\_path不是字符串，抛出TypeError。
- 如果加载失败，抛出RuntimeError。
