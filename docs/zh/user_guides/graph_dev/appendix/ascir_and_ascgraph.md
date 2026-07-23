# AscIR与AscGraph

## 简介

AscendC IR（如下简称AscIR）是一套基于Ascend C语言定义的IR，专门负责自动融合软件栈的后端工作，AscIR与Ascend C API一一对应：AscIR的类型（type）用于区分对应的Ascend C API；AscIR定义中的输入、输出、属性等内容，分别映射到Ascend C API的各类参数。

从[融合原理](../autofuse/overview.md)中得知，如果融合范围用FusedGraph来表达，FusedGraph内部包含\>=1个AscBackend节点，一个AscBackend节点对应一个AscGraph，而一个AscGraph内包含多个AscIR节点。下面分别介绍AscIR和AscGraph。

## AscIR

一个AscIR有如下部分组成：

- **类型（type）**：类型确定了AscIR计算类型，是区分不同IR的根本信息。
- **输入、输出**：定义输入输出的个数、每个输入输出的含义。
- **属性**：严格来说，属性也属于AscIR的输入，但是与输入不同的是，在构造AscGraph图时，属性值需要被确定下来，在后续的执行过程中，属性值不允许变化。

比如，有如下IR定义：

```c++
REG_ASC_IR(Load)
    .Input("T")
    .Output("T")
    .Attr<Expression>(offset, 0)
    .DataType("T", {DT_FLOAT, DT_FLOAT16});
```

上述代码定义了一个类型为Load的IR，该IR：

- 有一个输入，类型可以为DT\_FLOAT、DT\_FLOAT16两种。
- 有一个输出，该输出的类型与输入相同。
- 有一个属性，属性名为offset，该属性的数据类型是int64\_t，默认值是0。

除了上述信息，AscIR通常需要提供如下额外的信息：支持的输入输出内存硬件类型（内存硬件类型，是指GM或UB构图时，需要严格满足AscIR定义的所有要求，否则会导致后续流程出错）。一般来说，AscIR分为两类：

- 计算类：包括Load、Add等真正表达某类硬件单元参与工作的。
- 内存类：表达一块内存，例如Data、Output、Workspace等。

## AscGraph

### 概念介绍

在当前算子实现模型中，完整计算逻辑分为Host部分（通常称为Tiling函数）和Device部分（通常称为Kernel）。执行算子时，首先根据输入shape等信息运行Host侧的Tiling函数，生成TilingData，并将其作为Kernel的输入，继续执行Device部分。Device侧一份Kernel代码在多个硬件单元（也被称为核、block）上并发执行，因此，每个核只负责整个算子计算的一部分。逻辑上，Tiling函数负责为当前shape选择最高效的Kernel实现，并为选定Kernel计算出合适的分核等策略，结果通过TilingData传递至Kernel，指导Kernel正确运行。

自动融合的结果是GE图中的AscBackend或FusedAscBackend（FusedAscBackend也是携带一个子图对象，子图对象类型是ComputeGraph，ComputeGraph内部有一个或者多个AscBackend节点）节点，它仅作为自动融合软件栈生成的“壳”节点，真正的计算逻辑存储在节点的AscGraph属性中。AscGraph采用DAG（有向无环图）作为基础数据结构，基于符号化能力，AscGraph可以完整表达算子的计算逻辑（包括分核策略）：基于AscGraph中核内计算部分，Codegen生成单核Kernel代码；基于完整AscGraph，Auto Tiling生成Tiling函数；基于AscGraph上的符号信息，产生TilingData定义、InferShape推导函数。

AscGraph具有两种形态：

- [HintGraph](#hintgraph)：融合框架的输出，作为后端输入，仅描述算子的计算逻辑。
- [ImplGraph](#implgraph)：Auto Schedule处理后的结果，作为Codegen和Auto Tiling块的输入，完整描述算子的实现逻辑，包括Schedule策略、内存管理、流水并行等所有信息。

虽然HintGraph和ScheduleResult在表达层次和用途上有所不同，它们共享相同的构图基础逻辑，并复用部分字段，同时也各自定义了一些特定字段。但是，**同一个字段在不同形态下的语义保持一致**，不会因形态不同而改变其含义。

#### AscGraph的结构

AscGraph表达多层循环中的多步计算：每一步计算对应图中的一个节点，而节点则由AscIR实例化而来，节点之间的有向边表示数据的传递关系，因此AscGraph是一张DAG。每个节点还包含属性，用于指定该计算所处的循环层级。

**实例化**过程指将AscIR转换为AscGraph节点的步骤，可类比为“类”到“对象”的创建过程。此过程中，AscIR的输入、输出以及属性都会被具体赋值，使节点具备可执行性。例如，若AscIR类型为Cast，则在实例化时会设置dst\_dtype属性，用以指明本次类型转换要将输入数据转换为哪种数据类型。

**属性**是由AscIR规范所定义的字段，可存在于图或节点上，既可以是简单数值，也可以是包含更多子字段的容器（类似C语言的struct）。

#### 节点

节点是AscIR实例化后的实体，其输入、输出及属性完全遵循AscIR的定义。 在AscGraph中，每个节点代表一次计算操作，并嵌套于循环结构中进行多次执行。

- **节点的计算语义**

    节点的完整计算语义由如下几部分共同组成：

  - **节点类型**：节点的计算类型，如Add代表加法。
  - **节点属性**：用于补充描述计算逻辑的必要信息。例如，Cast节点的dst\_type属性指明输入数据需要转换的目标数据类型。
  - **输入、输出的描述**：有些节点需要输入输出的上述信息共同描述计算逻辑，例如Broadcast需要借助输入输出的repeats和axis确定在哪个轴发生了广播，Cast需要借助输出的dtype确定要将输入转换为哪种数据类型。

- **节点的属性**

    每个AscGraph节点都包含以下在各个形态（HintGraph和ImplGraph）中均生效的通用属性：

  - **name**：节点名称，在AscGraph内唯一。如果存在多张AscGraph（例如ScheduleResult阶段的多张ImplGraph，或者HintGraph阶段，FusedAscBackend中的多个AscGraph），多张AscGraph之间，节点名字允许重复。
  - **type**：节点类型，表示该节点对应的AscIR类型。
  - **sched**：定义节点的执行方式，包含**sched.axis属性**，表示该节点所属的循环层级，详细说明参见[轴](#轴)说明。
  - **其他AscIR定义的专有属性**：节点还包含[AscIR](#ascir)中定义的IR特有属性。

- **输入与输出的属性**

    每个节点的输入、输出都具有属性，AscGraph要求**同一条边的两端，输入与输出属性值必须完全相同**。因此，在多数情况下，只需关注节点的**输出属性**，因为相连节点的输入属性必然相同。以下是各个形态中均适用的通用属性：

  - **axis**：该输出包含的轴数量，所有轴必须引用AscGraph全局已定义的轴。
  - **repeats**：每个轴上的数据重复次数，即该输出在各轴上的大小。
  - **stride**：该输出在各轴上的索引步长（stride）。

- **节点的执行方式**

    在AscGraph中，节点通常嵌套在多层循环中，被循环多次执行。例如，假设Foo节点的调度信息如下：

    ```text
    Foo.sched.axis = [z0, z1, z2, z3];
    ```

    这意味着Foo节点将在**s0 \* s1 \* s2 \* s3**次迭代中被执行，即每个循环变量z0、z1、z2、z3依次遍历其对应的大小s0、s1、s2、s3，等价于如下C++ 代码：

    ```c++
    for (int64_t i0 = 0; i0 < s0; ++i0) {  // 遍历 `z0` 轴
        for (int64_t i1 = 0; i1 < s1; ++i1) {  // 遍历 `z1` 轴
            for (int64_t i2 = 0; i2 < s2; ++i2) {  // 遍历 `z2` 轴
                for (int64_t i3 = 0; i3 < s3; ++i3) {  // 遍历 `z3` 轴
                    // 执行 Foo 计算
                }
            }
        }
    }
    ```

#### 轴

轴（Axis）是AscGraph中极为重要的概念，AscGraph对循环与数据的表达均依赖轴来进行定义。轴代表数据的一个维度，并可通过name或id进行标识，其核心属性size用以描述该维度的长度。轴及其大小的符号作为属性保存在AscGraph上，是图级定义，因此轴可被图中的任意元素（如节点）通过轴的id引用，以实现对同一维度的共享认知与引用。

在图上创建轴，举例来说，创建一根长度为s0的轴：

```c++
AscGraph graph;

// 允许轴的长度为变量，变量名为`s0`
Expression s0 = graph.CreateSizeVar("s0");

// 创建一根名字为`z0`的轴，轴的长度为 `s0`，轴的`id`在创建时分配，可以通过`z0.id`获取
Axis &z0 = graph.CreateAxis("z0", s0);
```

在自动融合的命名习惯中，通常使用**s前缀表示大小变量**，例如s0、s1分别表示第0个、第1个大小变量。s取自**symbol**（符号）的首字母，表示这是一个符号化表达的变量。

轴的命名通常以**z前缀**，例如z0、z1，并遵循以下对应关系：z0的大小一般由s0表示，z1的大小由s1表示，依此类推。这种命名方式确保了**轴（z）与其大小（s）之间的映射关系清晰可读**，方便理解。

通过轴表达循环，比如，当前AscGraph有四根轴：

```c++
AscGraph graph;
Expression s0 = graph.CreateSizeVar("s0");
Expression s1 = graph.CreateSizeVar("s1");
Expression s2 = graph.CreateSizeVar("s2");
Expression s3 = graph.CreateSizeVar("s3");

Axis &z0 = graph.CreateAxis("z0", s0);
Axis &z1 = graph.CreateAxis("z1", s1);
Axis &z2 = graph.CreateAxis("z2", s2);
Axis &z3 = graph.CreateAxis("z3", s3);
```

当循环为z0、z1、z2、z3时，表达**依次、全量**遍历这四根轴，等价的C语言表达为：

```c++
for (int64_t i0 = 0; i0 < s0; ++i0) {  // 遍历 `z0`轴，遍历长度为`z0`轴的长度`s0`
    for (int64_t i1 = 0; i1 < s1; ++i1) {  // 遍历 `z1`轴
        for (int64_t i2 = 0; i2 < s2; ++i2) {  // 遍历 `z2`轴
            for (int64_t i3 = 0; i3 < s3; ++i3) {  // 遍历 `z3`轴
            }
        }
    }
}
```

#### 图的输入和输出

AscGraph通过特定类型的节点表达输入和输出，其对应的AscIR：

- **Data类型的AscIR**表示图的输入。
  - Data AscIR无输入，只有一个输出，对应图的某个输入。
  - 其int32\_t index属性用于指示该输入在图中的序号。
  - 每个Data节点对应一个图输入，与其相连的节点即是读取该输入的算子。

- **Output类型的AscIR**表示图的输出。
  - Output AscIR无输出，只有一个输入，对应图的某个输出。
  - 其int index属性用于指示该输出在图中的序号。
  - 每个Output节点对应一个图输出，与其相连的节点即是整图的最终输出算子。

**与常规节点的输入、输出不同，Data的输出、Output的输入分别代表AscGraph的输入、输出，是AscGraph对外部的承诺，在整个后端运行过程中，不允许被修改。**

### HintGraph

HintGraph对应后端入口的阶段，准确来说，HintGraph是Auto Schedule模块输入的规范。在HintGraph阶段，认为所有内存是无限的（包括UB与GM）内存，每个节点是个独立循环，完成对输入的处理；同时，HintGraph阶段不考虑分核，认为所有算子在一个核上完成计算。比如，有如下HintGraph：

![图示](../figures/hintgraph.png)

按照节点的内存语义，Load输出、Add输入输出、Store输入应该位于UB内存，Data的输出为GM内存。而UB内存是有限的，Data输出的数据不可能被全部加载到UB后，再继续向后计算Add。而由于HintGraph阶段不关注内存大小，因此上图仍然是正确的。

HintGraph阶段，对AscGraph增加了一些要求和约定，当这些约定无法满足、又需要表达时，可以使用FusedAscBackend类型的Ascend IR，分隔成多个AscGraph表达：

#### 连续性约定

- 显式Broadcast：如果计算发生Broadcast操作，需要显示表达出，即必须在构图中加上Broadcast的算子。
- 集中Broadcast：如果发生这种操作，需要在load后不做任何其他运算，立即进行。
- 单轴Broadcast：如果发生多轴广播，那么每个Broadcast节点最多广播一根轴。

#### 一套循环轴

一张AscGraph图上，只能有一套循环轴，即图上的所有节点，sched.axis必须相同。

#### Reduction总是keep\_dim

Reduction类操作，不存在keep\_dims参数，总是认为keep\_dims=true。

### ImplGraph

在Auto Schedule处理HintGraph的过程中，会基于多种Schedule策略生成一到多份ScheduleResult，每份ScheduleResult都完整表达了算子的实现逻辑。一个ScheduleResult的整体计算会被拆分成一到多步子计算，每个子计算步骤被称为ScheduleGroup，其含义为独立应用Schedule策略。在一个ScheduleGroup中，每个Schedule策略均会产生一张新的AscGraph，该图被称为ImplGraph。

每个ImplGraph还包含Schedule策略、核间、核内切分方式、内存、向量化等。

#### 轴变换

一个轴允许被切分为两个子轴，切分出的两根轴被称为内轴与外轴。比如将repeats为s0的z0轴切分后，产生z0Outer和z0Inner两个轴，两个轴的repeats相乘与s0相等，因此两轴的repeats可以表示为：ceil\(s0/s0I\)与s0I。

轴切分分为两类，Block切分与Tile切分：

- Block切分表示将数据切分后，分配到多个核上并行执行。Block切分的外轴、内轴一般命名方式是在原轴名字后加B/b，一个ImplGraph上，只能有一个Block外轴。比如将z0做Block切分，切分后外轴、内轴的名字分别为z0B和z0b。
- Tile切分为普通切分，不涉及特殊语义。

两个或以上的**连续循环轴**可以被合并成一个，所谓连续循环轴，是指在sched.axis中连续的轴，比如，若sched.axis=\[z0, z1, z2, z3\]，则\[z1, z2, z3\]为连续轴。如果轴发生了切分，例如变成\[z0, z1T, z1t, z2T, z2t, z3\]，则\[z1t, z2T\]为连续轴。

如果为循环轴做过reorder，例如上述例子中，顺序变为\[z0, z2, z1, z3\]，则\[z0, z2\]为连续轴。若将切分后的轴做了reorder，则**不能**对reorder部分的切分后轴做合并。例如上述例子中，对z1做了切分和reorder，顺序变为\[z0, z1T, z2, z1t, z3\]，则\[z2, z1t\]、\[z1t, z3\]均不是连续轴，也就无法进行合并，在本例中，\[z0, z1T\]的顺序没有被reorder影响，仍然可以被合并。

在轴属性上，有如下字段表达轴的变换关系：

- **axis.type**：表示本轴由哪种轴变换类型产生，如果产生一根轴经历了多次变换，那么本type仅保存最后一次的变换类型。
  - **kAxisTypeOriginal**：原始轴，未做过拆分或合并。
  - **kAxisTypeBlockOuter**：Block切分后的外轴。
  - **kAxisTypeBlockInner**：Block切分后的内轴。
  - **kAxisTypeTileOuter**：tile切分后的外轴。
  - **kAxisTypeTileInner**：tile切分后的内轴。
  - **kAxisTypeMerged**：多个轴合并后的轴。

- **from**：如果本轴是被切分而来，那么from中保存被切分轴；如果本轴是由多个轴合并而来，那么from中保存合并前的轴。
- **split\_pair**：如果本轴是被切分而来，那么split\_pair中保存同一次切分时产生的另外一根轴。

#### 向量化和循环轴

在ImplGraph中，对轴的定义做了扩展，表达了三种含义，考虑一个API的调用：

```python
// 共有四根轴`z0, z1, z2, z3`，长度分别为`s0, s1, s2, s3`，索引时使用`q + index`
for q0 in range(s0):
    buffer[s1 * s2 * s3];
    for q1 in range(s1):
        // inputs 包含四根轴：q0, q1, q2, q3，本次调用计算`q2, q3`两根轴的数据
        buffer[index] = CalcApi(inputs[q0][q1])
```

上述调用共包含如下几项信息：

- 循环层数：q0、q1
- 一次计算的数据长度：s2 \* s3
- 计算后，数据存储buffer的长度：例子中，buffer包含z1、z2、z3三个轴，因此长度为三个轴长度的乘积。

通过如下字段表达：

|位置|属性key|含义|对应本例中的值|
|--|--|--|--|
|节点上|**sched.axis**|节点上的所有轴|[z0, z1, z2, z3]|
|节点上|**sched.loop_axis**|循环停止轴，停止轴及其前面的轴为循环部分，停止轴后面的部分为一次计算的数据量|z1|
|输出上|**vectorized_axis**|数据存储buffer的长度|[z1, z2, z3]|
|输出上|**vectorized_stride**|存储数据时的stride|[s2*s3, s3, 1]|
