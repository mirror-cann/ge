# bundle\_build\_model

## 产品支持情况

全量芯片支持。

## 功能说明

将输入的一组Graph编译为适配AI处理器的离线模型，并保存到内存缓冲区，该接口适用于权重更新场景。

## 函数原型

```python
bundle_build_model(graph_with_options: List[GraphWithOptions]) -> ModelBuffer
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| graph_with_options | 输入 | 待编译的一组图和编译参数。该入参为一个结构体，包括如下参数：<br>class GraphWithOptions:<br>   graph: Graph<br>   build_options: Dict[str, str] = field(default_factory=dict)<br>一组图包括：权重初始化图，权重更新图，推理图，其中只有推理图支持设置如下options参数。<br>可以通过传入options参数配置离线模型编译配置信息，当前支持的配置参数请参见[aclgrphBuildModel支持的配置参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/API/ascendgraphapi/atlasgeapi_07_0143.html)，key为C++常量对应的字符串值。 |

## 返回值说明

\(ModelBuffer\)编译生成的离线模型缓存。

## 约束说明

- 如果graph\_with\_options不是List\[GraphWithOptions\]类型，抛出TypeError。
- 如果编译失败，抛出RuntimeError。
