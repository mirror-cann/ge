# 基于Pattern匹配实现Pass（Python）

本文档介绍如何使用Python开发自定义融合Pass。Python融合Pass与C++融合Pass使用同一套GE匹配替换机制，但Python更适合快速开发和运行时接入，能够显著降低开发成本。与C++实现的主要区别如下，两者在核心机制上完全一致，都遵循"匹配-决策-替换"的三步流程，但Python实现在以下方面具有显著优势：

| 对比维度 | Python实现 | C++实现 |
| --- | --- | --- |
| 开发模式 | 直接加载.py文件，无需编译 | 需要编译成.so动态库 |
| Pattern定义 | 支持@pattern装饰器，可用表达式直接描述 | 使用EsGraphBuilder构建Pattern图 |
| Replacement定义 | 简单替换可直接return，表达式即replacement | 必须手动创建replacement graph |
| 迭代速度 | 修改文件后重新触发执行或加载即可验证 | 修改后需要重新编译、安装 |
| 接入方式 | 配置ASCEND_GE_PY_PASS_PATH环境变量 | 编译后放到指定目录 |
| 适用场景 | 快速原型开发、规则验证、迭代调试 | 生产环境、性能要求高 |

整个融合行为的逻辑同样分为三个步骤：

1. **匹配**：通过Pattern来定义一种图结构，进行子图结构的匹配查找。
2. **决策**：根据匹配结果，结合更具体的条件，判断该匹配是否满足融合要求。
3. **替换**：在确认可融合后，执行图结构的替换操作，完成优化。

逻辑架构图如[图1](./implement_pass_using_pattern_matcher_cpp.md#fig1)所示。开发者通过继承GE提供的基类并重写其方法来实现自定义融合Pass，根据应用场景的不同，Python语言下，GE也提供了两类基类供继承：

- **通用子图融合（1:1或复杂拓扑替换）场景**：适用于需匹配完整子图结构并整体替换为另一子图的场景

    继承[PatternFusionPass](../../../api/graph_engine_api/python/ge/passes/PatternFusionPass/overview.md)类实现自定义融合Pass类，并通过@[register\_fusion\_pass](../../../api/graph_engine_api/python/ge/passes/register_fusion_pass.md)装饰器将Pass注册到指定阶段。

- **单节点替换（单节点替换为N个节点）场景**：继承[DecomposePass](../../../api/graph_engine_api/python/ge/passes/DecomposePass/overview.md)类实现自定义融合Pass类，并通过@[register\_decompose\_pass](../../../api/graph_engine_api/python/ge/passes/register_decompose_pass.md)装饰器将Pass注册到指定阶段。

两种场景在使用方法上类似，区别在于匹配对象的粒度（子图/单节点）与替换方式（复杂拓扑替换/多节点展开），下面将详细展开介绍。

## 场景介绍

- **通用子图融合（1:1或复杂拓扑替换）场景融合Pass开发**

    本节首先对该场景下涉及的核心数据结构PatternFusionPass进行介绍，其次介绍开发过程中需要重写的核心方法，最后介绍如何将Pass注册到指定阶段。**PatternFusionPass**类声明如下：

    ```python
    class PatternFusionPass:
        def patterns(self) -> List[Pattern]:
            ...

        def meet_requirements(self, match_result: MatchResult) -> bool:
            return True

        def replacement(self, inputs, match_result: MatchResult = None):
            ...
    ```

    开发者通过继承PatternFusionPass类并重写核心方法实现自定义融合Pass的开发。函数介绍如下：

    | 方法 | 说明 | 是否必须重写 |
    | --- | --- | --- |
    | [patterns](../../../api/graph_engine_api/python/ge/passes/PatternFusionPass/patterns.md) | 定义要找什么：<br>  - 方式一@pattern装饰的方法：定义在目标图中匹配的模板拓扑；使用Python表达式描述Pattern。@pattern装饰器适合大多数常见拓扑，它会自动capture已访问的外部输入和return的pattern输出，不会自动capture未作为输出返回的中间Tensor。<br>  - 方式二：手动构建Pattern图，返回Pattern列表。如果meet_requirements或replacement需要读取未作为return输出的中间Tensor，应使用手动构建Pattern的方式，并调用capture_tensor方法标记需要读取的中间Tensor。<br>如果只是表达Add(x, 0)、MatMul+Add这类规则，优先用@pattern，代码更短也更贴近优化逻辑。 | 是，两种方式二选一 |
    | [meet_requirements](../../../api/graph_engine_api/python/ge/passes/PatternFusionPass/meet_requirements.md) | 判断能不能替换：对Pattern匹配到的图结构按条件进行过滤；输入匹配结果，返回布尔值。 | 否，默认返回True |
    | [replacement](../../../api/graph_engine_api/python/ge/passes/PatternFusionPass/replacement.md) | 定义替换成什么：定义替换结构；输入inputs对象和match_result，返回替换后的结构。 | 是 |

  - **Patterns**
    - **使用@pattern装饰器（推荐）**

        使用@pattern装饰器可以大幅简化Pattern的定义，直接用Python表达式描述图结构：

        ```python
        @register_fusion_pass(name="MyPass", stage=PassStage.BEFORE_INFER_SHAPE)
        class MyPass(PatternFusionPass):
            @pattern
            def my_pattern(self, inputs):
                return inputs[0] + inputs[1]  # 表达式即pattern

            def meet_requirements(self, match_result):
                # 条件过滤（可选）
                return True

            def replacement(self, inputs):
                return inputs[0]  # 表达式即replacement
        ```

    - **手动构建Pattern（与C++一致）**

        当需要更细粒度地控制Pattern定义时，可以手动构建Pattern图，方式与C++一致：

        ```python
        from ge.es.graph_builder import GraphBuilder
        from ge.passes import create_pattern, create_replacement


        def patterns(self):
            builder = GraphBuilder("pattern")
            a, b, c = builder.create_inputs(3)
            matmul = MatMul(a, b)
            add = matmul + c
            pat = create_pattern(builder.build_and_reset([add]))
            pat.capture_tensor(matmul)
            pat.capture_tensor(add)
            return [pat]


        def replacement(self, match_result):
            builder = GraphBuilder("replacement")
            a, b, c = builder.create_inputs(3)
            gemm = GEMM(a, b, c, builder.create_scalar_float(1.0), builder.create_scalar_float(1.0))
            return create_replacement(builder.build_and_reset([gemm]))
        ```

  - **meet\_requirements**

    对于Patterns获取到的匹配结果，在meet\_requirements中进行筛选。每个match\_result类型的匹配结果作为meet\_requirements的入参，通过match\_result开发者可以对匹配结果的信息进行筛选，最后返回的布尔值作为是否替换该匹配结果的依据。

    ```python
    def meet_requirements(self, match_result):
        # 遍历匹配结果中的所有节点
        for node in match_result.get_matched_nodes():
            # 如果当前节点是常量节点
            if node.type == "Const":
                return _is_zero(node.get_attr("value"))
        return False
    ```

    match\_result是匹配结果类，包含匹配结果的节点、连边等信息。开发者可以使用match\_result成员函数获取匹配结果的相关信息以进行筛选：

    | 方法 | 说明 |
    | --- | --- |
    | [get_matched_nodes](../../../api/graph_engine_api/python/ge/passes/MatchResult/get_matched_nodes.md)() | 获取匹配到的所有节点列表。 |
    | [get_captured_tensor](../../../api/graph_engine_api/python/ge/passes/MatchResult/get_captured_tensor.md)(index, node_io) | 获取指定索引的捕获Tensor。 |

    以下是使用get\_captured\_tensor成员函数校验ReLu输出是否为动态shape的示例：

    ```python
    def meet_requirements(self, match_result):
        from ge.graph.types import TensorDesc

        # 尝试从match_result中获取第一个捕获的输出张量
        try:
            relu_output = match_result.get_captured_tensor(0)
            # 获取输出张量描述信息
            relu_out_tensor_desc = relu_output.node.get_output_desc(relu_output.index)
            # 检查是否为动态shape
            if relu_out_tensor_desc.shape.size != -1:
                return False
            return True
        except Exception:
            return False
        ```

  - **replacement**

    replacement中定义目标结构，替换与Patterns中匹配且`meet_requirements`返回True的部分。Python提供了两种定义replacement的方式：

    - **方式一：直接返回表达式（简单场景）**

        对于简单的替换场景，可以直接return表达式，GE会自动构建replacement graph：

        ```python
        def replacement(self, inputs):
            # 直接返回第0个输入，相当于删除某个节点
            return inputs[0]
        ```

        或者使用算子表达式：

        ```python
        def replacement(self, inputs):
            a, b, c = inputs[:3]
            # 返回GEMM表达式
            return GEMM(a, b, c, 1.0, 1.0)
        ```

    - **方式二：复杂场景（同时接收`inputs`和`match_result`）**

        如果既想使用表达式简化replacement构建，又需要读取匹配节点的属性，可以同时接收两个参数：

        ```python
        def replacement(self, inputs, match_result):
            a, b, c = inputs[:3]
            transpose_a = False
            transpose_b = False
            for node in match_result.get_matched_nodes():
                if node.type not in ("MatMul", "BatchMatMulV2"):
                    continue
                try:
                    transpose_a = bool(node.get_attr("transpose_x1"))
                    transpose_b = bool(node.get_attr("transpose_x2"))
                except RuntimeError:
                    pass
                break
            return GEMM(a, b, c, 1.0, 1.0, transpose_a, transpose_b)
        ```

    **注册自定义融合Pass**

    使用`@register_fusion_pass`装饰器把类注册给GE：

    ```python
    @register_fusion_pass(name="MyPass", stage=PassStage.BEFORE_INFER_SHAPE)
    class MyPass(PatternFusionPass):
        ...
    ```

    name必须唯一，[PassStage](../../../api/graph_engine_api/python/ge/passes/PassStage.md)表示执行阶段；初次开发建议使用PassStage.BEFORE\_INFER\_SHAPE，因为replacement后还能进入GE后续统一的shape推导流程。

- **单节点替换（单节点替换为N个节点）场景融合Pass开发**

    该场景下的Pass继承的基类为DecomposePass。由于被替换结构是单个节点，Pattern不再需要手动定义，而是在注册时通过op\_types参数指定算子类型：

    ```python
    from ge.passes import DecomposePass, PassStage, register_decompose_pass

    @register_decompose_pass(
        name="PythonMyDecomposePass",
        stage=PassStage.AFTER_INFER_SHAPE,
        op_types=["Conv2D"],
    )
    class PythonMyDecomposePass(DecomposePass):
        def meet_requirements(self, node):
            return node.get_attr("groups") != 1

        def replacement(self, node):
            # 返回用基础算子组成的替换graph
            ...
    ```

    op\_types决定GE会把哪些类型的节点交给这个pass，meet\_requirements再判断其中哪些节点真的需要替换。

    **注册自定义融合Pass**：

    如下是使用装饰器[REG\_DECOMPOSE\_PASS](../../../api/graph_engine_api/python/ge/passes/register_decompose_pass.md)将Conv2D作为op\_types初始化CustomOne2NPass，并将其注册在kAfterInferShape的示例：

    ```python
    @register_decompose_pass(name="CustomOne2NPass", stage=PassStage.AFTER_INFER_SHAPE, op_types=["Conv2D"])
    class CustomOne2NPass(DecomposePass):
        ...

    ```

## 开发示例

此处以删除Add\(x, 0\)结构为例（使用@pattern装饰器），详细介绍如何通过Python自定义融合Pass修改Graph。完整样例代码可参见[AddZeroPass Python样例](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/4_add_zero_pass/python/README.md)。源码仓还给出了如下各种场景的样例，供用户参考：

- [MatMul+Add Python样例](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/1_fuse_matmul_add_pass/python/README.md)
- [capture tensor Python样例](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/2_fuse_matmul_add_pass_with_capture_tensor/python/README.md)
- [PatternMatcherConfig Python样例](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/3_fuse_matmul_add_pass_with_pattern_matcher_config/python/README.md)
- [DecomposePass Python样例](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/6_decompose_grouped_conv_to_splited_pass/python/README.md)

修改前后的图结构如下：

```text
x ----\
       Add ---- out      ==>      x ---- out
0 ----/
```

1. 引入必要的模块

    ```python
    from math import fabs

    from ge.graph.types import DataType
    from ge.passes import PassStage, PatternFusionPass, pattern, register_fusion_pass
    ```

2. 实现辅助函数

    ```python
    def _scalar_value(value):
        while isinstance(value, list):
            if len(value) != 1:
                return None
            value = value[0]
        return value


    def _is_zero(tensor):
        value = _scalar_value(tensor.data)
        if value is None:
            return False
        if tensor.data_type == DataType.DT_FLOAT:
            return fabs(float(value)) < 1e-6
        if tensor.data_type == DataType.DT_DOUBLE:
            return fabs(float(value)) < 1e-15
        if tensor.data_type == DataType.DT_INT32:
            return int(value) == 0
        return False
    ```

3. 实现自定义Pass

    ```python
    @register_fusion_pass(name="PythonAddZeroPass", stage=PassStage.BEFORE_INFER_SHAPE)  # 注册自定义融合Pass，并指定被调用的阶段

    class PythonAddZeroPass(PatternFusionPass):
        @pattern
        def add_zero(self, inputs):
            return inputs[0] + 0  # 定义要匹配的结构：第0个输入加0

        def meet_requirements(self, match_result):
            # 检查匹配到的常量节点是否真的为0
            for node in match_result.get_matched_nodes():
                if node.type != "Const":
                    continue
                return _is_zero(node.get_attr("value"))
            return False

        def replacement(self, inputs):
            return inputs[0]  # 直接返回第0个输入，相当于删除Add
    ```

    - **@pattern装饰器**：inputs\[0\]+0表达式描述了要匹配的结构：第0个外部输入加一个常量0。GE会自动识别这个表达式并构建对应的Pattern。
    - **meet\_requirements方法**：遍历匹配结果中的节点，找到Const节点并验证其值是否为0。只有满足条件的匹配才会被替换。
    - **replacement方法**：直接返回inputs\[0\]，表示用第0个输入替换整个Add节点，实现删除Add\(x, 0\)的效果。

## 如何使用自定义Pass

完成上述自定义Pass后，本节介绍如何将Python Pass加载到GE编译阶段。

1. 设置CANN环境变量

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

    其中，/usr/local/Ascend/为root用户的默认安装路径，请根据实际情况进行替换。

2. 设置加载Pass环境变量

    告诉GE从哪里加载Python Pass：

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/my_pass.py
    ```

    也可以指向目录：

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/pass_dir/
    ```

    多个路径用冒号分隔：

    ```bash
    export ASCEND_GE_PY_PASS_PATH=/path/to/a.py:/path/to/pass_dir/
    ```

    ASCEND\_GE\_PY\_PASS\_PATH详细说明请参见《环境变量参考》。

3. 执行编译：支持但不限于如下几种入口编译模型文件：

    如果要查看上述自定义Pass有没有生效，在编译模型前，需要dump图进行查看：在执行之前设置DUMP\_GE\_GRAPH（详细说明请参见《[环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)）环境变量，然后使用如下入口编译模型：

    - 离线编译：离线场景建议使用pyatc触发编译，pyatc和atc的命令行参数一致，但会在当前Python解释器进程中运行，便于加载Python Pass。命令如下：

        ```bash
        pyatc --model=./model.onnx --framework=5 --soc_version=xxx --output=./model
        ```

        --soc\_version取值查询方法请参见[atc命令](../overview/quick_start.md#使用atc命令转换onnx模型)。

        pyatc详细使用方法请参见[Pyatc 接口](../../../api/graph_engine_api/python/ge/pyatc/pyatc_interface.md)。

    - 在线编译：参考[样例程序运行](https://gitcode.com/cann/ge/blob/master/examples/fusion_pass/pattern_base_pass/4_add_zero_pass/cpp/README.md)章节，使用torch\_forward.py脚本执行在线推理。

## 结果验证

1. 启用图Dump

    建议每次开发都打开图dump：

    ```bash
    export DUMP_GE_GRAPH=1
    ```

2. 查看融合前后的图

    设置了dump环境变量后，程序执行完毕，会在当前路径生成ge\_onnx\*.pbtxt等图文件，用户可以获取如下两张图（以指定Pass执行阶段在InferShape之前为例），然后使用Netron等可视化软件查看：

    - ge\_onnx\_xxxx\_PreRunBegin.pbtxt：融合前的图
    - ge\_onnx\_xxxx\_RunCustomPassBeforeInfershape.pbtxt：融合后的图

    运行成功后，日志中会出现类似输出\(input\_0名字实际可能有区别\)：

    ```text
    [PythonAddZeroConstValueMatchPass] matched=PythonAddZeroConstValueMatchPass_add_zero_pattern captured=input_0:0
    [PythonAddZeroPass] matched=PythonAddZeroPass_add_zero_pattern captured=input_0:0 const_dtype=DT_FLOAT zero=True
    ```

如果Pass未命中，可以按照如下顺序进行排查：

| 现象 | 可能原因 | 检查方式 |
| --- | --- | --- |
| Python文件没被加载 | ASCEND_GE_PY_PASS_PATH没设置、路径不存在、后缀不是.py | 先确认环境变量和路径 |
| 类已加载但pass不执行 | 没有使用注册装饰器，或注册阶段不对 | 检查@register_fusion_pass/@register_decompose_pass |
| pattern不命中 | 算子类型、输入个数或输出边界不一致 | 对比dump图中的真实拓扑 |
| 命中了但不替换 | meet_requirements返回False | 打印命中节点属性 |
| 替换后图异常 | replacement输出没有覆盖外部消费者需要的Tensor | 回到[机制文档](https://gitcode.com/cann/ge/blob/master/docs/zh/design/features/fusion_pattern_pass.md)检查边界规则 |
