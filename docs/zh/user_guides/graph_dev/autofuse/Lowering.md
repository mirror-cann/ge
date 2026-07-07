# Lowering

## 简介

Lowering模块负责完成GE图上基于循环轴合并的算子融合。该模块输入是符号化推导后的GE图，输出则是包含融合算子的GE图。融合算子使用AscBackend类型的算子表达，AscBackend算子持有一个AscGraph类型的属性，用于表达其计算逻辑。在Lowering阶段之后，CanFuse/Schedule/Codegen等模块将进一步完成AscBackend节点间的二次融合、代码生成与编译工作。

Lowering模块包含两个层次，算子级Lowering和图级Lowering（Graph Lowering）。

- 算子级Lowering，使用低层级的IR进行算子计算逻辑的Scalar表达，Scalar表达指描述每个输出的值是如何计算得到的。可以进行Scalar表达的算子分为三类：
  - Pointwise类

    即输出shape为多个输入广播后的结果，每个输出的值由对应位置输入的值计算得出。常见的GE IR都为该类型，例如Add、Mul、Exp、Abs等

  - Reduction类

    输出的每个元素由多个输入元素经过Reduce计算得到。例如ReduceSum、ReduceMax。

  - View类

    输出的每个元素，等于输入中特定位置的一个元素，中间不进行计算。输出元素的个数可以与输入元素个数不同，例如Broadcast算子可以将多个输出元素对应到同一个输入元素。Lowering当前支持的View类包括Broadcast等。

- 图级Lowering，会依次调用图上算子的Lowering实现，并基于Scalar表达完成算子融合。图级Lowering会完成以下场景的融合：
  - Pointwise + Pointwise
  - Pointwise + View
  - Pointwise -\> Reduction

## 算子Lowering实现

算子Lowering就是使用Loop IR来表达算子的计算逻辑。以基础的Add节点为例，其计算过程主要包括：

1. 从输入Tensor中加载数据。
2. 对输入数据进行广播。
3. 判定计算采用的dtype，并将输入转换为对应类型。
4. 对加载到的数据进行Add计算。
5. 将结果写入节点的输出Tensor。

GE图编译时，使用节点Anchor表达运行时的Tensor输入，Anchor可以理解为Tensor输入的编译时占位符。使用Loop IR表达Add计算的伪代码示例如下：

```c++
graphStatus LoweringAdd(const ge::Node &node) {
    auto x = loop::Load(node.GetInDataAnchor(0)); // 从输入anchor0中加载数据
    auto y = loop::Load(node.GetInDataAnchor(1)); // 从输入anchor1中加载数据
    vector<Expression> broadcasted_shape = xxx; // 计算广播后的输出
    ge::DataType compute_dtype = xxx; // 计算类型提升后的计算dtype，注意，该步骤与Codegen时的主动类型提升不同
    x = loop::Broadcast(x, broadcasted_shape);
    x = loop::Cast(x, compute_dtype);
    y = loop::Broadcast(y, broadcasted_shape);
    y = loop::Cast(y, compute_dtype);
    auto result = loop::Add(x, y); // 表达计算
    loop::Store(node.GetOutDataAnchor(0), result); // 计算结果保存到输出anchor
}
```

由于加载输入、保存输出、计算广播和类型提升等操作具有高度的相似性，抽取公共实现后，一个算子的Lowering可以简化为仅实现计算部分：

```c++
Var LoopAdd(const vector<LoopVar> &inputs) { // Var为loop计算输出的中间结果类型
     return loop::Add(inputs[0], inputs[1]);
 }
```

最终LoopAdd会在Lowering过程中调用：

```c++
graphStatus LoweringAdd(const ge::Node &node) {
     // x, y = 公共逻辑，处理输入的加载、广播、类型转换
     auto result = LoopAdd(x, y); // 表达计算
     // 公共逻辑，处理输出的保存
 }
```

## Graph Lowering实现

Graph Lowering遍历图上的算子，依次调用算子的Lowering函数，如果算子没有Lowering函数，则会执行默认的Lowering函数。

算子Lowering函数原型示例如下，输入一个GE图上的算子节点，返回Lowering是否成功，并在内部完成节点的Scalar表达，将结果存储在属性上。

```c++
ge::graphStatus(const NodePtr &node);
```

Graph Lowering整体分为三步：

1. 依次调用每个节点的Lowering函数，获得其对应的Loop表达。
2. 在Loop表达上进行index和range的推导，从而将中间过程中的View类表达到输入数据的搬运方式上。
3. 结合Loop表达推导出的index和range信息，生成一套轴的AscGraph。

调用完节点Lowering函数后，并不会立即将节点转换为AscBackend融合节点类型，而是将每个节点的每个输出Tensor关联一个KernelBox对象：KernelBox用于表达一个算子输出Tensor的计算过程。其核心部分定义如下：

```c++
class KernelBox {
  public:
   bool IsExternKernel() const; // 是否是Extern类型（即需要走原始的GE IR实现），取值为true，表示走原始GE IR实现
    // 处理从该KernelBox中加载一个元素时的行为，对于Extern或者Realize后的KernelBox，等同于直接从其输出Anchor上Load数据。而对于能继续融合的KernelBox，则会返回其内部的计算过程
   LoopVar Load() const;
   // 进行Realize，即需要为该KernelBox生成AscBackend节点，如果为true，则表示可融合
   void Realize(bool persistent = true);
 };
```

### 单输出多引用

对于单输出多引用的算子，可能出现多个计算对其输出的加载方式不一致的情况。GraphLowering会基于重计算进行Lowering，即对于一个可以继续融合的节点，如果其输出被多个节点使用，例如下面例子中的第一个Abs，那么在使用该输出的每个节点对应的KernelBox中，均会包含第一个Abs的计算。通过重计算，能够处理同一份数据按照不同方式进行搬运的场景。

图中CSE（Common Subexpression Elimination，公共表达式消除） 是编译器优化、代码生成以及数学计算中常用的一种优化技术，其核心目标是识别并复用表达式中重复出现的子表达式，以减少冗余计算，提升效率。

**图 1**  单输出多引用流程
![图1](../figures/single_output_multi_ref_flow.png "单输出多引用流程")

### 融合与终止融合策略

在执行Lowering时，如何决定是否进行融合以及终止融合，本节给出当前的融合范围控制策略：

1. KernelBox的类型为Reduction类型。
2. KernelBox中的Loop节点总数超过阈值，当前阈值64。
3. KernelBox中的Load节点数量超过阈值，当前阈值4。
4. KernelBox作为一个无法Lowering的节点的输入（在默认Lowering函数中触发）。
5. KernelBox所属的节点包含输入或输出控制边（跨节点的Fuse会丢失准确的控制信息）。
6. KernelBox所属的节点的stream label信息与使用KernelBox作为输入的节点的stream label不一致。（如果任意一方未标记，则认为一致）。
7. 遇到Exp算子会立即Realize。
8. 控核scope不同核的算子不会融合到同一个融合算子。
9. 没有使用到的输入会被提前Realize。例如Zeroslike。
10. 如果算子带有\_super\_kernel\_scope属性，会直接跳过。
11. 带有\_disable\_autofuse\_scope会跳过Lowering。
12. 实际属性和算子IR不一致。
13. 算子的dtype校验如果不支持会跳过。
14. 纯scalar图会跳过融合（以单个Ascbackend为限）。

对于不支持Lowering的算子，会调用默认的Lowering实现，默认的Lowering函数会完成两个动作：

1. 将所有输入Anchor对应的KernelBox进行Realize，终止融合。
2. 将自身节点输出Anchor对应的KernelBox设置为ExternKernel类型的KernelBox。

当前使用默认Lowering实现的场景有三类，处理方式完全一致：

- 节点无法进行Lowering表达。
- 节点可以使用Lowering表达，但是未实现。
- 节点可以使用Lowering表达，且已经实现，但是由于无符号化推导结果，导致表达结果无效。

为节点编写Lowering实现时，无需考虑是否有符号化结果，因为会在Loop函数中进行判断。无论具体情况如何，最终Lowering后的KernelBox都是一致的，均为ExternKernel类型的KernelBox。

### 融合回滚策略

回滚是指在Lowering阶段，由于不感知具体的CanFuse/Schedule/Codegen能力，导致生成的AscBackend融合节点性能低于原始节点时，应该如何将该AscBackend节点回滚至原始节点去执行。当前的回滚策略包括：

如果AscBackend融合节点对应的原始GE IR节点数少于阈值（当前阈值为2），则回滚为原始节点执行。

## Lowering流程示例

本章节以一个简单的Conv+Abs+Exp+Conv的GE IR Graph为例（Conv无Lowering实现），简单介绍Lowering的流程。原始GE IR图如下：

**图 1**  原始GE IR图
![图1](../figures/original_ge_ir_graph.png "原始GE-IR图")

1. 首先会进行Conv的Lowering，由于Conv无注册的Lowering实现，会执行默认的Lowering：所有输出Anchor的KernelBox均为ExternKernel类型，表示执行原始的GE IR Kernel。

    **图 2**  输入Conv的Lowering
    ![图2](../figures/input_conv_lowering.png "输入Conv的Lowering")

2. 然后，Lowering Abs时加载输入数据，伪码如下：

    ```c++
    graphStatus LoweringAbs(const ge::Node &node) {
         auto x = loop::Load(node.GetInDataAnchor(0)); // 从输入anchor0中加载数据
         ...
         auto result = loop::Abs(x);
         ...
     }
    ```

    当执行loop::Load\(node.GetInDataAnchor\(0\)\)时，实际会调用对端输出Anchor关联的KernelBox的Load方法，此时为Conv节点输出，由于Conv的KernelBox为Extern类型，对其Load时，等同于对Conv节点的输出Anchor进行Load，因此Abs节点在Lowering后，其输出Anchor关联的KernelBox如下：

    **图 3**  Abs的Lowering
    ![图3](../figures/abs_lowering.png "Abs的Lowering")

3. 接着进行Exp节点的Lowering，伪码如下：

    ```c++
    graphStatus LoweringExp(const ge::Node &node) {
         auto x = loop::Load(node.GetInDataAnchor(0)); // 从输入anchor0中加载数据
         ...
         auto result = loop::Exp(x);
         ...
     }
    ```

    对其输入Load时，实际执行的是Abs输出Anchor关联的KernelBox的Load，其没有被Realize也非Extern（IsRealize为true，表示可融合；IsExternKernel为true，表示走原始GE IR流程）可继续进行融合，因此会返回其内部的计算过程（load-\>Abs-\>Store）。在Exp Lowering后，其输出Anchor关联的KernelBox如下：

    **图 4**  Exp的Lowering
    ![图4](../figures/exp_lowering.png "Exp的Lowering")

4. 最后，进行Conv节点的Lowering，执行默认的Lowering函数，会将所有的输入Anchor对端的KernelBox进行Realize（因为Conv要回退至原始GE IR Kernel执行，需要有输入）。

    **图 5**  输出Conv的Lowering
    ![图5](../figures/output_conv_lowering.png "输出Conv的Lowering")

完成Graph Lowering后，可以看出，需要Realize的KernelBox是Exp节点输出的KernelBox。此时，会将Exp节点输出Anchor关联的KernelBox转换为AscGraph，并将Exp节点替换为AscBackend类型。需要注意的是，Exp节点关联的KernelBox的输入为Conv:0而非Abs节点，因此在替换后，Abs节点会因输出未被任何节点使用而被删除。然而，为了确保在任意时刻都能方便地回滚到原始节点，无用节点的删除并不会立即执行。
