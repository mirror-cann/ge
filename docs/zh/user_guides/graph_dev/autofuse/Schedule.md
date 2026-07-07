# Schedule

自动融合的Schedule模块是连接计算定义与高效代码生成的核心组件，其核心能力在于对计算过程进行灵活的重排与优化：在不改变计算语义的前提下，通过计算重排、调度原语调用等方式灵活调整计算实现，以获得更佳的性能。优化手段包括循环合并、解空间生成、并行优化、内存优化及多模板生成等。

当用户通过AscIR定义一张描述Scalar计算逻辑的[HintGraph](../appendix/ascir_and_ascgraph.md#hintgraph)后，Schedule模块会基于硬件特性对计算进行调度优化，生成多张表达切分及内存关系的[ImplGraph](../appendix/ascir_and_ascgraph.md#implgraph)，为Codegen及Auto Tiling模块提供基础，支撑其生成高性能Kernel代码。

## 循环合并

循环合并是一种重要的循环变换技术，其核心作用是通过重构循环结构，减少内存访问次数、降低控制开销、提升数据局部性，并为后续优化铺路，最终在不改变计算结果的前提下提升程序执行效率。

例如，假设有如下两层循环:

```python
for i in range(N):
    for j in range(M):
        C[i][j] = A[i][j] + B[i][j]
```

Add是纯elewise操作，所以可以合并成一个线性循环：

```python
for fused in range(N * M):
    i = fused // M
    j = fused % M
    C[i][j] = A[i][j] + B[i][j]
```

对应到AscIR表达如下：

```text
z0 = graph.create_axis("z0", N)
z1 = graph.create_axis("z1", M)
Load load0("load_0");
load0.x = data0.y;
load0.attr.sched.axis = {z0.id, z1.id};
load0.y.axis = {z0.id, z1.id};
load0.y.repeats = {N, M};
load0.y.strides = {M, 1};

Load load1("load_1");
load1.x = data1.y;
load1.attr.sched.axis = {z0.id, z1.id};
load1.y.axis = {z0.id, z1.id};
load1.y.repeats = {N, M};
load1.y.strides = {M, 1};

Add add("add");
add.x1 = load0.y;
add.x2 = load1.y;
add.attr.sched.axis = {z0.id, z1.id};
add.y.axis = {z0.id, z1.id};
add.y.repeats = {N, M};
add.y.strides = {M, 1};
```

循环合并后：

```text
Load load0("load_0");
load0.x = data0.y;
load0.attr.sched.axis = {z0z1.id};
load0.y.axis = {z0z1.id};
load0.y.repeats = {N * M};
load0.y.strides = {1};

Load load1("load_1");
load1.x = data1.y;
load1.attr.sched.axis = {z0z1.id};
load1.y.axis = {z0z1.id};
load1.y.repeats = {N * M};
load1.y.strides = {1};

Add add("add");
add.x1 = load0.y;
add.x2 = load1.y;
add.attr.sched.axis = {z0z1.id};
add.y.axis = {z0z1.id};
add.y.repeats = {N * M};
add.y.strides = {1};
```

## 生成TilingCase

在自动融合技术中，tiling解空间生成是实现高效计算调度的关键环节，其核心目标是为复杂计算任务提供多样化的分块（tiling）策略选项，以便后续优化器从中筛选出最优解。简单来说，tiling解空间生成的过程可以理解为对输入数据或计算任务进行“分块可能性”的系统枚举，每一个解空间被称为一个TilingCase。

切分方式的设计与算子实现特性紧密关联，为了实现对多样化算子的系统性切分策略枚举，首先基于算子的实现特性将其抽象为9种compute\_type（如下图中的compute类和view类算子）。同一compute\_type类别的算子具有相似的计算逻辑与数据访问模式，因此可以共享一套tiling切分策略框架。

**图 1**  Tiling切分策略
![图1](../figures/tiling_split_strategy.png "Tiling切分策略")

为具象化这一策略框架，我们对算子的轴进行归一化分组，将所有轴统一划分为（xgroup、ygroup、rgroup）三个维度集合，具体定义如下：

- **xgroup**：专为Concat等视图类算子设计的分组。以Concat为例，会以Concat轴进行划分，将Concat轴前的轴划分到xgroup，将Concat轴及其以后的轴划分到ygroup。
- **ygroup**：对应Elementwise、Broadcast等类型算子的循环轴分组。
- **rgroup**：Reduce类操作通常对reduce轴有特殊的切分要求，因此会将所有reduce轴单独放入rgroup。

> [!NOTE]说明
>
>引入xgroup、ygroup、rgroup的核心原因是为了支持复杂场景下的“双切分”需求。例如，在包含reduce混合的计算图中，ygroup控制elementwise的循环切分，rgroup中的轴控制reduce方向的循环切分。

在完成单算子的轴分组后，需通过预设的合并规则（Merge）对计算图中所有算子的分组策略进行合并。合并结果将作为适用于全图所有算子的统一切分策略，为后续解空间生成提供基础。

通过上述分组与合并机制，可实现两大核心功能：

- 筛选出适用于计算图中所有节点的切分方式，形成有效的TilingCase。
- 通过判断不同AscGraph的tiling分组是否能成功合并，验证两张图的可融合性。

以下面case为例，介绍通过tiling分组合并生成TilingCase的原理：

```text
z0 = graph.create_axis("z0", s0)
z1 = graph.create_axis("z1", s1)
data = ascir.ops.Data('data', graph)
data.y.dtype = ascir.dtypes.float32
# 声明load算子
load = ascir.ops.Load('load')
load.attr.sched.axis = [z0, z1] # 调度轴
load.x = data.y
load.y.axis = [z0, z1] # tensor的输出轴
load.y.size = [s0, s1] # tensor输出大小
load.y.strides = [s1, 1] # tensor的输出步长
# 声明abs算子
abs = ascir.ops.Abs('abs')
abs.attr.sched.axis = [z0, z1]
abs.x = load.y
abs.y.axis = [z0, z1]
abs.y.size = [s0, s1]
abs.y.strides = [s1, 1]
# 声明max算子
max = ascir.ops.Max('max')
max.attr.sched.axis = [z0, z1]
max.x = abs.y
max.y.axis = [z0, z1]
max.y.size = [s0, 1]  # 对z1轴进行reduce操作
max.y.strides = [1, 0]
```

abs是一个elewise算子，每个轴在计算上并无差异，因此只要内存连续，可以将多根轴合并成一根轴进行切分：例如[图2](#fig2)所示，先在轴上做block切分，15被分到3个block上，block0为紫色的部分，在block0内再进行tiling分块，此时tiling块未占满block0分配的部分，因此还需要在block内加一个for循环。

**图 2**  elewise类算子切分<a id="fig2"></a>
![图2](../figures/elewise_op_split.png "elewise类算子切分")

reduce类的切分较为复杂，实现上需要双切分：行方向上是elewise轴，列方向上是reduce轴；首先在行方向上分block，block内写循环，然后在列方向上再加一层for循环。

**图 3**  reduce类算子切分
![图3](../figures/reduce_op_split.png "reduce类算子切分")

通过TilingGroup的合并规则:

```text
()(z0,z1)() Merge ()(z0)(z1) => ()(z0)(z1)
```

将abs算子原本同属ygroup的\(z0，z1\)轴进行拆分，其中z1轴被调整至rgroup，这一调整的核心目的是使abs算子与后续的reduce类型算子保持统一的切分策略。

## 并行优化

- 循环拆分

    循环拆分的核心作用是通过引入新的循环层级，明确划分出适合并行的外层循环和适合向量化的内层循环。

    针对每个TilingCase，会将xgroup、ygroup、rgroup中存在的轴切分成ub\_out和ub\_in：

    ![fig](../figures/auto_quantization_flow.png)

    例如：\{z0, z1, z2\}，切分在z1上，就把z1切分成z1T和z1t，z1T就是ub\_out，z1t就是ub\_in。

- 向量化

    向量化是利用硬件SIMD（Single-Instruction Multiple-Data stream processing，单指令流多数据流）单元提升数据并行计算效率的关键技术，通过将单元素操作转化为向量操作，显著减少指令执行次数并提高硬件利用率。

    例如，对于如下循环：

    ```c++
    for (int i = 0; i < 256; i++) {
        c[i] = a[i] + b[i];
    }
    ```

    非向量化执行需要256条加法指令，向量化后只需要一条加法指令。

    在每个组内选择一根轴作为ub切分轴后，会将ub\_in及其内轴作为向量化轴。此时，由于分组是按照xyr来生成的，按照这个顺序生成的向量化轴，轴序与内存排布不一致会引入非连续搬运，因此需要根据输出的轴序进行重排列。

    ![fig](../figures/auto_quantization_flow_8.png)

    例如，输出tensor轴序是\(a, b, c, d\)，轴分组是\(a, c\),\(b, d\)\(\)，按照这个顺序生成的向量化轴分组是\(a\_in, c, b\_in, d\)，需要调整成\(a\_in, b\_in, c, d\)。

    > [!NOTE]说明
    >
    >Schedule阶段全图统一设置了相同的向量化轴，对于部分API来说，由于指令等限制，并不能将所有向量化轴都进行向量化处理。此时，需要CodeGen阶段对无法向量化处理的轴进行外抛for循环处理。
    >例如向量化轴为\[z1,z2,z3\]，相当于3层循环，如果指令支持3层循环，则可以写成vector\[z1,z2,z3\]；但如果指令只能支持两层循环，则需要CodeGen给出如下的代码：
    >
    >```text
    >for (i in z1) {
    > vector[z2,z3]
    >}
    >```

- 循环合并、循环绑核

    循环合并与循环绑核二者常常配合优化，循环切分阶段在每个分组内都产生了ub\_out的轴。

    1. 将所有非reduce轴合并为一根轴，所有reduce轴合并为一根轴以减少循环嵌套层数。
    2. 对合并后的循环进行拆分，得到外层和内层。
    3. 将拆分后的外层循环绑定到多个block上以实现并行，内层通过循环进行消化。

    ![fig](../figures/auto_quantization_flow_9.png)

    例如：\{z0, z1T, z1t，z2\}，会先把z0、z1T合轴成z0z1T，再在z0z1T上进行多核切分，由于z0z1T可能会超过参与计算的逻辑AI Core核数，因此在分完核后还要多一层循环，z0z1T会被进一步拆成z0z1TB和z0z1Tb，值的具体大小由Auto Tiling在Tiling阶段计算得出。

## 内存优化

目前主要依据节点引用关系进行内存复用，为了提升复用效果，会尽量将占用大小相近的内存分配到同一个group内，然后在group内进行复用。

内存复用伪代码如下：

```c++
for (node in all nodes) {
  for (output in node.outputs) {
    // 标记tensor的依赖数
    output->sch.depends = output->anchor->GetPeerInDataNodesSize();
    // try reuse from free queue
  }
  for (input in node.inputs) {
    input->sch.depends--;
    if (input->sch.depends == 0) {
        Enqueue(input->opt.reuse_id); // 标记为freeTensor，可以被后续节点复用
    }
  }
}
```

对于部分API来说，输出可以直接复用输入，针对这一类API，可以采用Inplace复用，即输出直接复用输入内存。如下图所示，Inplace复用前需要3块内存：

**图 4**  内存复用前
![图4](../figures/memory_reuse_before.png "内存复用前")

Inplace复用后只需要2块内存：

**图 5**  内存复用后
![图5](../figures/memory_reuse_after.png "内存复用后")

## 多模版生成

针对一张计算图，可能存在多种实现方式。以尾轴concat为例，可以在UB上将多个小包做ub\_concat先组成大包再完整搬出，也可以直接转成非连续搬运在GM（Global Memory，全局内存）上完成重排。前者在小shape场景可以显著提高MTE（Memory Transfer Engine，AI Core的数据传递引擎）搬运效率，从而获得更好的性能优势。但ub\_concat也存在需要内轴全载的限制，导致某些场景下无法使用。在Schedule阶段无法确定选择哪个模板时，通常会生成一个适用于任意shape的通用模板，以及特定场景下的性能优化模板，由Auto Tiling模块在tiling阶段决定具体使用哪个模板。

- UB concat模板：

    ![fig](../figures/fake_quant_model.png)

- 改图模板：

    ![fig](../figures/fake_quant_model_10.png)
