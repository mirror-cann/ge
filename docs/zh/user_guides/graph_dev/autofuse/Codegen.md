# Codegen

Codegen的功能是解析Schedule生成的ImplGraph，生成实际可执行的代码，包括在Host侧（Host Code）和Device侧（Kernel Code）运行的代码，其中：

- Host Code，Host侧执行的代码，包括如下内容：
  - tiling\_func：负责将计算任务划分为更小的块（tiling），便于在Device上高效执行。
  - infer\_shape：推断各个tensor的形状，确保后续计算的正确性。
  - get\_kernel：获取并生成Device侧的kernel代码。

- Kernel Code：Device侧执行的kernel代码，包括如下内容：
  - kernel：具体的计算逻辑。
  - tiling\_data：用于描述如何划分和处理数据的结构。

Codegen的输入是Schedule生成的ImplGraph，该图包含了shape信息、节点信息、轴信息等生成代码的基本要素。ImplGraph示例如下：

**图 1**  ImplGraph示例
![图1](../figures/implgraph_example.png "ImplGraph示例")

图上的具体信息如下所示：

```text
// size描述计算量大小
Sizes:
  z0z1t_size: VAR
  z0z1Tb_size: VAR

// axis描述轴信息，包括大小、类型、对齐方式等
Axis:
  z0(0) : 200, ORIGINAL, align: -1, allow_oversize_axis: 0, allow_unaligned_tail: 1
  z1(1) : 200, ORIGINAL, align: -1, allow_oversize_axis: 0, allow_unaligned_tail: 1
  z0z1(2) : 40000, ORIGINAL, align: -1, allow_oversize_axis: 0, allow_unaligned_tail: 1 // ORIGINAL代表未切分的原始轴
  z0z1T(3) : Ceiling((40000 / (z0z1t_size))), TILE_OUT, from: {z0z1, }, align: 1, allow_oversize_axis: 0, allow_unaligned_tail: 1
  z0z1t(4) : z0z1t_size, TILE_IN, from: {z0z1, }, align: 1, allow_oversize_axis: 0, allow_unaligned_tail: 1 // TILE_IN代表UB切分的内轴
  z0z1TB(5) : Ceiling((Ceiling((40000 / (z0z1t_size))) / (z0z1Tb_size))), BLOCK_OUT, from: {z0z1T, }, align: 1, allow_oversize_axis: 0, allow_unaligned_tail: 1
  z0z1Tb(6) : z0z1Tb_size, BLOCK_IN, from: {z0z1T, }, align: 1, allow_oversize_axis: 0, allow_unaligned_tail: 1 // BLOCK_IN代表核间切分的内轴

// node，每个节点会生成kernel代码中的api调用
Nodes:
......
  abs_test/gather_0: Load (1)
......
  abs_test/abs_0: Abs (2)
    .axis = {z0z1TB, z0z1Tb, z0z1t, }
    .loop_axis = z0z1Tb
    .api:
      .compute_type = elewise
      .type = Compute
      .unit = Vector
    .x = abs_test/gather_0.y
    .y.dtype = float32
    .y.axis = {z0z1TB, z0z1Tb, z0z1t, }
    .y.repeats = {(40000 / (z0z1Tb_size * z0z1t_size)), z0z1Tb_size, z0z1t_size, }
    .y.strides = {(z0z1Tb_size * z0z1t_size), z0z1t_size, 1, }
    .y.vectorized_axis = {z0z1t, } // 向量化轴代表UB内计算的数据量对应的轴
    .y.vectorized_strides = {1, }
    .y.mem:
      .tensor_id = 3
      .alloc_type = Queue
      .hardware = UB
      .position = TPosition::VECOUT
    .y.que:
      .id = 1
      .depth = 2
      .buf_num = 2
      .reuse_id = 1
  abs_test/store: Store (3)
......
```

根据ImplGraph解析，以下示例代码展示了Codegen生成DataCopy API的过程，可以看出，核心流程包括解析图上的切分策略，组装API的入参，生成在Device侧执行的代码。

```c++
ss << "DataCopyPadExtend(" << ub << ", " << gm << "[" << gm_offset << " + " << tpipe.tiler.Size(api_attr.offset)
   << "], " << dma_param.block_count << ", " << dma_param.block_len << ", " << dma_param.src_stride << ", "
   << dma_param.dst_stride << ");" << std::endl;
```

与传统算子代码类似，针对不同的切分策略，Schedule会生成多个模板，对应不同的ImplGraph，Codegen需要依次解析这些模板，生成模板函数，并在核函数入口处根据tiling\_key调用不同的模板函数。示例如下：

```c++
extern "C" __global__ __aicore__ void abs_test(GM_ADDR abs_test_Data_0, GM_ADDR abs_test_Output_0, GM_ADDR workspace, AutofuseTilingData param) {
  const AutofuseTilingData t;
    if (t.tiling_key == 0) {
      abs_test_0_general_0_nil_2_nil(abs_test_Data_0, abs_test_Output_0, workspace, t);
    } else if (t.tiling_key == 1) {
      abs_test_0_general_0_nil_2_nil_unaligned(abs_test_Data_0, abs_test_Output_0, workspace, t);
    }
}
```

上面AutofuseTilingData结构体示例如下，其中的成员变量具体值，包括tiling\_key等由ATT计算给出：

```c++
BEGIN_TILING_DATA_DEF_T(AutofuseTilingData)
  const uint32_t block_dim = 40;
  const uint32_t corenum = 0;
  const uint32_t ub_size = 196352;
  const uint32_t hbm_size = 0;
  const uint32_t tiling_key = 0;
  const uint32_t z0z1z2z3t_size = 17968;
  const uint32_t z0z1z2z3Tb_size = 57;
  const uint32_t q0_size = 96;
  const uint32_t q1_size = 35936;
  const uint32_t q2_size = 35936;
  const uint32_t b0_size = 35936;
  const uint32_t tmp_tbuf_size = 8192;
END_TILING_DATA_DEF_T;
```

不同的模板函数，生成的for循环以及每次处理的数据量各不相同，示例如下：

- tiling\_key=0

    对z1轴进行切分，每次在UB中完成计算的向量化轴为z1t。

    ```c++
    for (int z0z1Tb = 0; z0z1Tb < z0z1Tb_loop_size; z0z1Tb++) {
        Abs(y_local[0], xlocal[0], z1t_actual_size);
    }
    ```

- tiling\_key=1

    对z0轴进行切分，每次在UB中完成计算的向量化轴为z0t以及z1。

    ```c++
    for (int z0Tb = 0; z0Tb < z0Tb_loop_size; z0Tb++) {
        Abs(y_local[0], xlocal[0], z0t_actual_size * z1_axis_size);
    }
    ```

总体而言，Codegen是依据ImplGraph生成kernel代码的，但要持续提升kernel代码的性能，仍需在Codegen框架优化和API内部优化两方面不断挖掘。
