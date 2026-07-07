# 简介

Session类用于管理图的执行会话。

会话执行调用示例：

```python
from ge.session import Session
from ge.graph import Tensor
from ge.ge_global import GeApi

# 初始化 GE
config = {
    "ge.execDeviceId": "0",
    "ge.graphRunMode": "0"
}
GeApi.ge_initialize(config)


# 创建会话
session = Session()

# 添加图
session.add_graph(0, graph)

# 准备输入
input_tensor = Tensor(data=[1.0, 2.0, 3.0, 4.0], data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND, shape=[2, 2])

# 运行图
outputs = session.run_graph(0, [input_tensor])

# 终结 GE
GeApi.ge_finalize()
```

**Session接口产品支持情况如下：**

- Ascend 950PR/Ascend 950DT：支持
- Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
- Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
- Atlas 200I/500 A2 推理产品：不支持
- Atlas 推理系列产品：支持
- Atlas 训练系列产品：支持
- MC62CM12A AI处理器：不支持
- BS9SX2A AI处理器：不支持
- BS9SX1A AI处理器：不支持
- IPV350：不支持
