# DataFlow运行

## 简介

该章节将介绍如何构建并执行一张FlowGraph，包含以下两种方式：

- 通过注解构图并执行，即用户可以在UDF定义上添加装饰器@pyflow，会自动生成fnode构图函数，从而用户可通过UDF定义进行构图，使用更加便捷。
- 通过API构图并执行，即用户可以通过FlowNode相关的API直接构图。

## 通过注解构图并执行

通过注解构图并执行的流程如下所示。

**图1**  通过注解构图并执行  
![](figures/通过注解构图并执行.png "通过注解构图并执行")

示例代码如下。

```python
import dataflow as df
import numpy as np

@df.pyflow
def add(in0, in1):
    return in0 + in1

# 系统初始化
options = {
    "ge.exec.deviceId":"0",
    "ge.experiment.data_flow_deploy_info_path":"./data_flow_deploy_info.json",
    "ge.socVersion": "AscendXXX"  # 根据环境修改version
}
df.init(options)

# 定义FlowData
data0 = df.FlowData()
data1 = df.FlowData()
# 通过注解UDF生成FlowNode
add_node = add.fnode()

# 构建FlowData和FlowNode的连边关系
add_out = add_node(data0, data1)

# 通过FlowOut构建FlowGraph
dag = df.FlowGraph([add_out])

# 根据图自动生成部署策略，所有节点均部署在numa_config中的第1个设备上
# 如需修改需要注释掉下面这行代码，并修改./data_flow_deploy_info.json文件内容
df.utils.generate_deploy_template(dag, "./data_flow_deploy_info.json")

# 调用FlowGraph.feed填充输入
dag.feed({data0:np.array([[1, 2]], dtype=np.int32), data1:np.array([[2, 3]], dtype=np.int32)})

# 调用FlowGraph.fetch获取输出
print("dataflow fetch result:", dag.fetch())
# 打印：dataflow fetch result: ([array([[3, 5]], dtype=int32)], 0)

# 释放系统资源 
df.finalize()
```

## 通过API构图并执行

通过API构图并执行的流程如下所示。

**图1**  通过FlowNode构图并执行  
![](figures/通过FlowNode构图并执行.png "通过FlowNode构图并执行")

示例代码如下。完整示例代码请参考[sample\_base](https://gitcode.com/cann/ge/blob/master/examples/dflow/python/sample_base.py)。

```python
import dataflow as df
import numpy as np

# 系统初始化
options = {
    "ge.exec.deviceId":"0",
    "ge.experiment.data_flow_deploy_info_path":"./data_flow_deploy_info.json",
    "ge.socVersion": "AscendXXX"  # 根据环境修改version
}
df.init(options)

# 定义FlowData
data0 = df.FlowData()
data1 = df.FlowData()

# 定义FuncProcessPoint实现Add功能并添加到FlowNode中
pp0 = df.FuncProcessPoint(compile_config_path='config/add_func.json')
add_node = df.FlowNode(input_num=2, output_num=1)
add_node.add_process_point(pp0)

# 构建FlowData和FlowNode的连边关系
add_out = add_node(data0, data1)

# 通过FlowOut构建FlowGraph
dag = df.FlowGraph([add_out])

# 调用FlowGraph.feed_data填充输入
flow_info = df.FlowInfo()
dag.feed_data({data0:np.array([[1, 2]], dtype=np.int32), data1:np.array([[2, 3]], dtype=np.int32)}, flow_info)

# 调用FlowGraph.fetch_data获取输出
print("dataflow fetch result:", dag.fetch_data())

# 释放系统资源 
df.finalize()
```
