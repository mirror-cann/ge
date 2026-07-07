# 术语

**表 1**  术语表

|术语|含义|
|--|--|
|Ascend IR|Ascend Intermediate Representation，昇腾AI处理器专用的、用于表达计算流程的抽象数据结构。在本文档中，若无特殊说明，IR默认指代Ascend IR。|
|Block|Block在不同场景下具有多种含义，通常情况下指AI Core的逻辑核。典型场景有：AI Core逻辑核：一个Block表示一个AI Core的逻辑核，其BlockID是以0为起始的逻辑编号。DataBlock：一个DataBlock表示一条NPU矢量计算指令处理的数据单元，大小通常为32字节，一条指令可同时处理多个DataBlock。基本块：表示一次计算需要的典型数据块大小。|
|Broadcast|广播，一种张量操作机制。通过广播，较小的张量可以自动扩展以匹配较大的张量的形状。|
|Device|Device指安装了昇腾AI处理器的硬件设备，利用PCIe接口与主机Host侧连接，为Host提供神经网络计算能力。若存在多个Device，多个Device之间的内存资源不能共享。|
|Host|指与设备端Device相连接的X86服务器、ARM服务器，会利用Device提供的NN（Neural-Network ）计算能力，完成业务。|
|Global Memory/GM|设备端的主内存，AI Core的外部存储，用于存储大规模数据，但需要优化访问模式以提升性能。|
|Kernel|核函数，是Device设备上执行的并行函数。|
|MTE|Memory Transfer Engine，AI Core的数据传递引擎|
|OP|算子（Operator，简称OP），是深度学习算法中执行特定数学运算或操作的基础单元，例如激活函数（如ReLU）、卷积（Conv）、池化（Pooling）以及归一化（如Softmax）。通过组合这些算子，可以构建神经网络模型。|
|Tiling|Tiling指数据的切分和分块。计算数据量较大时，需要将数据进行多核切分、每个核也需要分块多次计算。|
|TilingData|TilingData指数据切分和分块的相关参数（如每次搬运的块大小、循环次数）。鉴于设备端Scalar计算能力限制，一般Tiling参数在Host侧计算完成，然后传输到设备侧供Kernel函数使用。|
|TilingFunc|算子工程提供的在Host侧计算Tiling的默认函数。|
|Unified Buffer/UB|AI Core内部存储单元，主要用于矢量计算，与逻辑内存AscendC::TPosition::VECIN、AscendC::TPosition::VECOUT、AscendC::TPosition::VECCALC相对应。|
