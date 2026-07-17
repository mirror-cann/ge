# ENABLE\_DYNAMIC\_SHAPE\_MULTI\_STREAM

## 功能描述

计算图执行时，开启多流并发执行功能在一定场景下可提升网络性能。当前多流并发执行功能默认关闭，动态shape图模式场景下，若开发者想开启多流并发执行功能，可通过此环境变量开启。

- 0：关闭多流并发执行功能，默认值。
- 1：开启多流并发执行功能。

## 配置示例

多流并发执行功能默认关闭，如果需要开启，请设置此环境变量的值为“1”。

```bash
export ENABLE_DYNAMIC_SHAPE_MULTI_STREAM=1
```

## 使用约束

当动态shape图里存在静态子图，图编译config中single stream的配置优先级高于ENABLE\_DYNAMIC\_SHAPE\_MULTI\_STREAM环境变量。

换言之，若PyTorch图模式（TorchAir）的config开启了单流执行，同时开启了多流执行的环境变量，以前者为准，图模式配置示例如下：

```python
config.ge_config.enable_single_stream = True
```

## 产品支持情况

全量芯片支持
