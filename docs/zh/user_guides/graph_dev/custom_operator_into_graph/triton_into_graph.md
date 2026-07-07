# Triton入图

## 简介

Triton是近年来广受开发者欢迎的一种Python化编程语言。开发者只需关注Tile/Block的切分方式及基于其上的运算逻辑，Triton编译器便能结合底层硬件特性，在编译过程中自动完成内存分配、数据搬运、计算调度和流水并行等复杂操作。因此，算子开发难度显著降低，开发效率大幅提升。

Triton-Ascend是面向昇腾平台构建的Triton编译框架，旨在让Triton代码能够在昇腾硬件上高效运行。Triton-Ascend还在不断完善中，后续将不断提升Triton Python API完备度、数据类型支持度、访存方式灵活性等，并持续优化编译器的自动优化能力，提升Triton-Ascend整体的功能与性能泛化性。

对于用户而言，将Triton算子集成到GE图中，可复用GPU训练时开发的Triton自定义算子，以最小成本将其纳入图执行流程，从而享受到图下沉带来的极致性能收益。

整图流程如下图所示。

![图示](../figures/triton_irgraph.png)

该特性主要分为两个阶段：开发阶段和运行阶段：

- 开发阶段：
    1. 用户需完成环境配置。详情请参见[环境准备](#环境准备)。
    2. 将Triton算子kernel编译为对应的npubin文件。详情请参见[生成npubin文件](#生成npubin文件)。
    3. [开发入TensorFlow图的交付件](#开发入tensorflow图的交付件)。当前仅支持TensorFlow框架。
    4. 创建Triton算子入GE图交付件，该步骤包括创建算子入图工程、开发算子入图交付件、编译部署。详情请参见[开发入GE图交付件](#开发入ge图交付件)。

- 运行阶段：用户可按照正常方式调用TensorFlow自定义算子，借助GE将模型解析为图，经过图编译、图执行等操作，即可充分利用图下沉机制，实现性能的显著提升。详情请参见[结果验证](#结果验证)。

## 开发流程

本节给出Triton入图的详细开发流程。

### 环境准备

- **CANN软件包安装**：参见[环境准备](../overview/environment_setup.md)安装CANN软件包，并设置环境变量。
- **Triton-Ascend 安装**：参见[Triton-Ascend安装指南](https://gitcode.com/Ascend/triton-ascend/blob/master/docs/sources/getting-started/installation.md)进行安装。

详细Sample示例请单击[Triton自定义算子入图样例](https://gitcode.com/cann/ge/tree/master/examples/custom_op/triton_add_custom)获取。

### 生成npubin文件

用户需调用Triton-Ascend提供的compile接口对算子kernel进行编译，生成对应的npubin文件。以自定义算子AddCustom为例（文件名举例为AddCustom.py），样例代码如下：

```python
# Compute Kernel
import torch
import torch_npu   # torch_npu插件

import triton
import triton.language as tl  # Triton语言模块

# 定义一个Triton内核函数，用于实现x+y的向量加法
@triton.jit
def add_kernel(x_ptr,  # 指向第一个输入向量的指针
               y_ptr,  # 指向第二个输入向量的指针
               output_ptr,  # 指向输出向量的指针
               n_elements,  # 向量的长度
               BLOCK_SIZE: tl.constexpr,  # 每个程序处理的元素数量
               ):
    # 获取当前程序的ID
    pid = tl.program_id(axis=0)  # We use a 1D launch grid so axis is 0.
    # 计算当前程序处理的起始位置
    block_start = pid * BLOCK_SIZE
    # 生成当前程序处理的元素的偏移量列表
    offsets = block_start + tl.arange(0, BLOCK_SIZE)
    # 创建一个掩码，用于防止越界访问
    mask = offsets < n_elements
    # 从DRAM加载x和y，使用掩码避免越界
    x = tl.load(x_ptr + offsets, mask=mask)
    y = tl.load(y_ptr + offsets, mask=mask)
    # 计算x+y
    output = x + y
    # 将结果写回DRAM
    tl.store(output_ptr + offsets, output, mask=mask)
# 定义块大小
BLOCK_SIZE_VALUE = 1024
# 创建Triton AST源代码对象
add_kernel_ast = triton.compiler.ASTSource(
    fn=add_kernel,
    signature={
        'x_ptr': '*fp32',
        'y_ptr': '*fp32',
        'output_ptr': '*fp32',
        'n_elements': 'i32',
    },
    constants={
        'BLOCK_SIZE': BLOCK_SIZE_VALUE  # 定义块大小为常量
    }
)
# 编译内核
compiled = triton.compile(add_kernel_ast)
```

在上述脚本路径执行如下命令：

```bash
python3  AddCustom.py
```

执行成功后，在\~/.triton/cache目录生成npubin文件、json文件、ttir文件、ttadapter文件。如下图所示。

**图 1**  cache目录文件
![图1示例](../figures/cache_directory_files.png "cache目录文件")

### 开发入TensorFlow图的交付件

TensorFlow本身支持自定义算子，因此需要提供一个TensorFlow自定义算子的入图交付件，让TensorFlow认识这个自定义算子，以TensorFlow1.15版本为例，创建原型注册文件_custom\_add\_custom.cc_，内容如下：

```c++
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/common_shape_fns.h"

using namespace tensorflow;
// 通过TensorFlow提供的REGISTER_OP接口完成算子原型的注册
REGISTER_OP("AddCustom")                                    // TensorFlow注册算子名
    .Input("x: T")                                          // 算子原型，输入参数x，类型为T
    .Input("y: T")                                          // 算子原型，输入参数y，类型为T
    .Output("z: T")                                         // 算子原型，输出参数z，类型为T
    .Attr("T: {float, double, int32}")                      // T类型支持范围
    .SetShapeFn(shape_inference::BroadcastBinaryOpShapeFn); // 算子shape信息推导，BroadcastBinaryOpShapeFn为TensorFlow提供的内置函数，输出shape信息由输入shape传播推导，即输入和输出shape保持一致


// TensorFlow自定义算子的CPU实现
class AddCustomOp : public OpKernel {
public:
    explicit AddCustomOp(OpKernelConstruction* context) : OpKernel(context) {}
    // 当前算子不支持CPU设备，实现该函数以抛出异常，提示该算子不支持CPU设备
    void Compute(OpKernelContext* context) override {
        OP_REQUIRES(context, false, errors::Unimplemented("AddCustomOp is not supported on CPU"));
    }
};

// 注册TensorFlow自定义算子的CPU实现
REGISTER_KERNEL_BUILDER(Name("AddCustom").Device(DEVICE_CPU), AddCustomOp);
```

使用如下命令对上述代码进行编译，编译成功后，在当前路径的outputs目录，生成名为libcustom\_ops.so的产物，后续的算子调用脚本中可通过load\_op\_library接口加载该so为python模块，从而调用自定义算子。

```bash
#! /bin/bash
SCRIPT_DIR=$(dirname "(realpath "$0")")
cd $SCRIPT_DIR || exit

rm -rf outputs
mkdir outputs

TF_CFLAGS=( $(python3 -c 'import tensorflow as tf; print(" ".join(tf.sysconfig.get_compile_flags()))') )    // 获取TensorFlow编译选项
TF_LFLAGS=( $(python3 -c 'import tensorflow as tf; print(" ".join(tf.sysconfig.get_link_flags()))') )       // 获取TensorFlow链接选项

SOURCE_FILES=$(find . -name '*.cc')                                                                         // 包含TensorFlow算子注册和CPU内核实现的cc文件

g++ -std=c++14 -shared $SOURCE_FILES -o outputs/libcustom_ops.so -fPIC ${TF_CFLAGS[@]} ${TF_LFLAGS[@]} -O2  // 编译命令，产物为libcustom_ops.so，TensorFlow即可通过load_op_library加载该so为python模块，调用自定义算子
```

### 开发入GE图交付件

1. 下载Triton算子入图的工程到任意目录，其内部结构如下：

    ```text
    custom_op/
    ├── src/
    |   └── custom_op.cpp               // 自定义算子源码文件
    ├── CMakeLists.txt                  // cmake文件
    ├── gen_npu_supported_ops_json.sh   // 生成文件脚本
    ├── README.md                       // Readme
    └── run.sh                          // 运行脚本
    ```

2. 进入上述自定义工程的src目录，编写custom\_op.cpp文件，示例如下：

    其中，AddCustom为用户自定义Triton算子的实现类，继承自[EagerExecuteOp](../../../api/graph_engine_api/cpp/ge/EagerExecuteOp/EagerExecuteOp.md/)，重点实现[Execute](../../../api/graph_engine_api/cpp/ge/EagerExecuteOp/Execute.md)函数。

    主要流程为：

    1. 从npubin中加载算子二进制。
    2. 获取kernel句柄。
    3. 构造kernel参数结构体。
    4. 调用acl  Launch Kernel接口，启动对应算子的计算任务。

    ```c++
    #include "graph/custom_op.h"
    #include "acl/acl_rt.h"
    #include "exe_graph/runtime/eager_op_execution_context.h"
    using namespace ge;
    class AddCustom : public EagerExecuteOp {
     public:
      graphStatus Execute(gert::EagerOpExecutionContext *ctx) {
        // 从npubin中获取kernel句柄
        const char *bin_path = "./add_kernel.npubin";        // 自定义算子的二进制文件路径
        aclrtBinHandle bin_handle = nullptr;                 // 二进制的句柄
        aclrtFuncHandle func_handle = nullptr;               // 核函数的句柄
        // 配置二进制加载选项
        aclrtBinaryLoadOption binary_load_option;
        aclrtBinaryLoadOptions binary_load_options;
        binary_load_option.type = ACL_RT_BINARY_LOAD_OPT_LAZY_LOAD;  // 指定解析算子二进制、注册算子后，是否加载算子到Device侧
        binary_load_option.value.isLazyLoad = 0U;

        binary_load_options.numOpt = 1;
        binary_load_options.options = &binary_load_option;

        // 加载算子二进制文件
        aclError ret = ACL_ERROR_NONE;
        ret = aclrtBinaryLoadFromFile(bin_path, &binary_load_options, &bin_handle);
        if (ret != ACL_ERROR_NONE) {
          std::cerr << __FILE__ << ":" << __LINE__ << " aclError: " << ret << std::endl;
        }
        // 从二进制中获取名为"add_kernel"的核函数句柄
        ret = aclrtBinaryGetFunction(bin_handle, "add_kernel", &func_handle);
        if (ret != ACL_ERROR_NONE) {
          std::cerr << __FILE__ << ":" << __LINE__ << " aclError: " << ret << std::endl;
        }

        // 获取算子输入Tensor
        const gert::Tensor *input_x = ctx->GetInputTensor(0);
        const gert::Tensor *input_y = ctx->GetInputTensor(1);
        // 获取Data地址
        void *x_addr = input_x->GetAddr();
        void *y_addr = input_y->GetAddr();

        // 为输出Tensor申请Device内存
        StorageShape &output_shape = input_x->GetShape();
        size_t tensor_size = input_x->GetSize();
        DataType data_type = input_x->GetDataType();
        const StorageFormat &format = input_x->GetFormat();
        gert::Tensor *output_z =
            ctx->MallocOutputTensor(0, output_shape, format, data_type, tensor_size, gert::TensorPlacement::kOnDeviceHbm);
        void *z_addr = output_z->GetAddr();

        // 获取需处理的元素个数和grid
        int64_t n_elements = input_x->GetShapeSize();
        int32_t grid_x = 16;
        int32_t grid_y = 1;
        int32_t grid_z = 1;
        int32_t block_num = grid_x * grid_y * grid_z;

        // 拼装args，构造kernel参数结构体
        // args的前3个参数和后3个参数是固定的，中间的是用户自定义的，要求是和kernel函数的签名以及类型严格一致
        // 按照当前的样例中的Kernel实现，要求AddCustom的两个输入的shape必须相同，不支持BroadCast的方式，例如Shape1 = [2,3,4],Shape2=[4]，这种就不支持
        struct __attribute__((packed)) {
          // void *ffts_addr __attribute__((aligned(8)));  // 注意：如果设备是Atlas A3 训练系列产品/Atlas A3 推理系列产品，则需要加上这个参数
          void *sync_block_lock __attribute__((aligned(8)));
          void *workspace_addr __attribute__((aligned(8)));
          const void *arg0 __attribute__((aligned(8)));
          const void *arg1 __attribute__((aligned(8)));
          void *arg2 __attribute__((aligned(8)));
          int32_t arg3 __attribute__((aligned(4)));
          int32_t grid_x __attribute__((aligned(4)));
          int32_t grid_y __attribute__((aligned(4)));
          int32_t grid_z __attribute__((aligned(4)));
        } args = {
           // nullptr, // 如果设备是Atlas A3 训练系列产品/Atlas A3 推理系列产品则需要传递这个参数
            nullptr, nullptr, input_x->GetAddr(), input_y->GetAddr(), z_addr, static_cast<int32_t>(n_elements),
        };
        // 获取stream
        void *stream = ctx->GetStream();
        // 启动核函数（异步执行）
        // 函数原型：
        // aclError aclrtLaunchKernelWithHostArgs(aclrtFuncHandle func_handle,
        //                                      uint32_t block_num,
        //                                      aclrtStream stream,
        //                                      aclrtLaunchKernelCfg *cfg,
        //                                      void *hostArgs,
        //                                      size_t size,
        //                                      aclrtPlaceHolderInfo *placeHolderArray,
        //                                      size_t placeHolderNum);
        // func_handle[in]:核函数句柄
        // block_num  [in]:指定核函数在几个核上运行
        // stream     [in]:任务 stream
        // cfg        [in]:任务下发的配置信息，不需要可填 nullptr
        // hostArgs   [in]:核函数参数地址
        // size       [in]:参数所占字节数大小
        // placeHolderArray[in]:占位数组
        // placeHolderNum[in]:占位数组的个数
        ret = aclrtLaunchKernelWithHostArgs(func_handle, static_cast<uint32_t>(block_num), stream, nullptr,
                                            static_cast<void *>(&args), sizeof(args), nullptr, 0);
        if (ret != ACL_ERROR_NONE) {
          std::cerr << __FILE__ << ":" << __LINE__ << " aclError: " << ret << std::endl;
        }
        return GRAPH_SUCCESS;
      }
    };
    // 注册该自定义算子，使其在图编译时能被识别和调用，注册名 "AddCustom" 将对应图中的算子名称，需与前端配置一致
    REG_AUTO_MAPPING_OP(AddCustom);
    ```

    其中，aclrtBinaryLoadFromFile、aclrtBinaryGetFunction、aclrtLaunchKernelWithHostArgs接口详细说明请参见[《Runtime运行时 API》](https://hiascend.com/document/redirect/CannCommunityruntimeapiaclkernel)中的“Kernel加载与执行”。

    GetAddr、GetShape、GetSize、GetDataType、GetFormat、GetShapeSize接口详细说明请参见[《基础数据结构和接口》](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。

    其他接口详细说明请参见[EagerExecuteOp](../../../api/graph_engine_api/cpp/ge/EagerExecuteOp/EagerExecuteOp.md/)、[Execute](../../../api/graph_engine_api/cpp/ge/EagerExecuteOp/Execute.md)、[GetInputTensor](../../../api/graph_engine_api/cpp/gert/EagerOpExecutionContext/GetInputTensor.md)、[MallocOutputTensor](../../../api/graph_engine_api/cpp/gert/EagerOpExecutionContext/MallocOutputTensor.md)、[GetStream](../../../api/graph_engine_api/cpp/gert/EagerOpExecutionContext/GetStream.md)、[REG\_AUTO\_MAPPING\_OP](../../../api/graph_engine_api/cpp/ge/REG_AUTO_MAPPING_OP.md)。

3. 开发完成后，执行custom\_op下的run.sh脚本进行编译：

    ```bash
    bash run.sh
    ```

    若提示如下信息，则表示构建成功：

    ![图示](../figures/triton_irgraph_1.png)

    按照上面的提示设置环境变量：

    ```bash
    export ASCEND_CUSTOM_OPP_PATH=/home/your_path/TritonOp/custom_op/build_out:$ASCEND_CUSTOM_OPP_PATH
    ```

### 结果验证

按照上述步骤全部执行完毕后，可以用如下的脚本实现入GE图的功能验证，以下是基于TensorFlow1.15版本的调用示例代码，假设文件名为run\_add\_custom\_tf\_1.15.py：

```python
#!/usr/bin/python3
# coding=utf-8

import os
import tensorflow as tf
import numpy as np
from npu_bridge.npu_init import *  # 导入NPU初始化模块
tf.enable_resource_variables()     # 启用资源变量

# np.allclose比较函数的相对公差参数
atol = 0.001
# np.allclose比较函数的绝对公差参数
rtol = 0.001

def main(unused_argv):
    custom_op_lib = tf.load_op_library(os.path.join("./outputs/libcustom_ops.so")) # 加载自定义算子库，可根据[开发入TensorFlow图的交付件](#开发入tensorflow图的交付件)编译结果目录进行修改
    # 定义输入数据
    shape_params = (8, 2048)
    dtype_params = np.float32

    # 生成两个随机输入张量，值在-2到2之间
    x_data = np.random.uniform(-2, 2, size=shape_params).astype(dtype_params)
    y_data = np.random.uniform(-2, 2, size=shape_params).astype(dtype_params)

    # 定义输入占位符（TensorFlow 1.x风格）
    x = tf.compat.v1.placeholder(dtype_params, shape=shape_params)
    y = tf.compat.v1.placeholder(dtype_params, shape=shape_params)

    # 定义两个加法操作
    tf_z = tf.math.add(x, y)                 # 调用TensorFlow原生算子（TensorFlow内置加法操作）
    ac_z = custom_op_lib.add_custom(x, y)    # 调用Triton编写的AddCustom自定义算子；

    # 配置Session以支持NPU加速
    config = tf.ConfigProto()
    custom_op = config.graph_options.rewrite_options.custom_optimizers.add()
    # 使用NPU优化器，开启动态编译模式
    custom_op.name = "NpuOptimizer"
    # 关闭remapping和memory optimization以避免干扰自定义算子执行
    config.graph_options.rewrite_options.remapping = RewriterConfig.OFF
    config.graph_options.rewrite_options.memory_optimization = RewriterConfig.OFF
    custom_op.parameter_map["compile_dynamic_mode"].b = True

    # 第一个Session：运行TensorFlow内置加法操作，获取参考结果（golden）
    with tf.Session(config=config) as sess:
        sess.run(tf.global_variables_initializer())   # 初始化变量
        tf_golden = sess.run(tf_z, feed_dict={x: x_data, y: y_data})  # 执行TensorFlow加法

    # 第二个Session：运行Triton自定义加法操作，获取结果
    with tf.Session(config=config) as sess:
        sess.run(tf.global_variables_initializer())    # 初始化变量
        ac_golden = sess.run(ac_z, feed_dict={x: x_data, y: y_data})  # 执行自定义加法

   # 将结果转换为指定数据类型，确保比较的一致性
    np.array(tf_golden).astype(dtype_params)
    np.array(ac_golden).astype(dtype_params)

    # 通过np.allclose函数比较TensorFlow和Triton的输出是否一致
    cmp_result = np.allclose(tf_golden, ac_golden, atol=atol, rtol=rtol)
    if cmp_result:
        print("The result of tf and ac is the same.")
    else:
        print("The result of tf and ac is different.")


if __name__ == '__main__':
    tf.app.run()
```

通过执行如下命令，运行上述文件：

```bash
python3 run_add_custom_tf_1.15.py > triton.log
```

执行完毕后，输入如下命令：

```bash
cat triton.log | grep "The result of tf and ac is the same"
```

如果结果中能搜索出如下信息，则表示用例执行成功：

![图示](../figures/triton_irgraph_2.png)
