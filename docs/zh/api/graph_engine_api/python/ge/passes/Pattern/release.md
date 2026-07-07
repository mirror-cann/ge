# release

## 产品支持情况

全量芯片支持。

## 功能说明

释放Python侧所有权，并将C++ Pattern\*句柄以整数形式返回。

## 函数原型

```python
release() -> int
```

## 参数说明

无

## 返回值说明

| 类型 | 说明 |
| --- | --- |
| int | 返回C++ Pattern*句柄对应的整数值。 |

## 约束说明

- 该方法为GE桥接层内部使用，Python Pass开发者无需直接调用。框架在patterns\(\)返回Pattern后会自动调用release\(\)。
- 成功的release每个实例最多只能调用一次。
- 调用后除is\_valid/release外的其他方法将抛出RuntimeError。
