# build\_finalize

## 产品支持情况

全量芯片支持。

## 功能说明

系统完成模型构建后，通过该接口释放资源。

## 函数原型

```python
build_finalize() -> None
```

## 参数说明

无。

## 返回值说明

无。

## 约束说明

该接口在[build\_initialize](build_initialize.md)之后调用，且仅能在进程退出时调用一次。
