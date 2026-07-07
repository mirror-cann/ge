# Graph中各类算子表达样例

## 定义数据节点（Data）

Graph的输入节点，也就是数据节点，使用Data算子实现。

Data算子原型定义：

```c++
REG_OP(Data)
    .INPUT(x, TensorType::ALL())
    .OUTPUT(y, TensorType::ALL())
    .ATTR(index, Int, 0)
    .OP_END_FACTORY_REG(Data)
```

根据Data算子原型定义创建Data算子实例，名称为data，初始化参数为desc\_data。同时通过“update\_input\_desc\_输入名称”和“update\_output\_desc\_输出名称”接口设置Shape、Format和Dtype，和用户需要处理的数据信息保持一致。

```c++
auto shape_data = vector<int64_t>({1,17,2,2});
TensorDesc desc_data(ge::Shape(shape_data), FORMAT_ND, DT_FLOAT);
auto data = op::Data("data");                // 创建Data算子
data.update_input_desc_x(desc_data);         // 设置算子输入描述
data.update_output_desc_y(desc_data);        // 设置算子输出描述
```

需要注意的是，定义数据节点时，**必须**通过“update\_input\_desc\_输入名称”和“update\_output\_desc\_输出名称”接口设置Shape、Format和Dtype。

## 定义常量节点（Const）

权值、偏置等信息为常量Tensor，可以通过Const算子实现。

Const算子原型定义：

```c++
REG_OP(Const)
    .OUTPUT(y, TensorType({DT_FLOAT, DT_FLOAT16, DT_BF16, DT_INT4, DT_INT8, DT_INT16, DT_UINT16, \
        DT_UINT8, DT_INT32, DT_INT64, DT_UINT32, DT_UINT64, DT_BOOL, DT_DOUBLE}))
    .ATTR(value, Tensor, Tensor())
    .OP_END_FACTORY_REG(Const)
```

### 直接构造权重数据

根据Const算子原型定义创建Const算子实例，初始值（即属性value的值）为weighttensor1。

```c++
// 构造weighttensor1
TensorDesc weight_desc(ge::Shape({1,3,3,3}), FORMAT_NCHW, DT_INT8);
int bs_size_weight = 27;
int8_t * bs_inputData_weight = nullptr;
bs_inputData_weight = new int8_t[bs_size_weight];
for (int i=0; i<bs_size_weight; ++i) {
    *(bs_inputData_weight+i) = 1;
}
Tensor weighttensor1(weight_desc, (uint8_t*)bs_inputData_weight, bs_size_weight*sizeof(int8_t));

// 创建Const算子，初始值（即属性value的值）为weighttensor1
auto weight1 = op::Const().set_attr_value(weighttensor1);
```

如果某个算子的原型输入和输出同名，代表这个是inplace操作算子，即算子的输出会更新输入。该场景下这种输入不能连接Const节点。

### 从文件读入权重数据

除了直接构造权重数据外，也可以直接从bin文件读入权重数据。

```c++
// 构造weight_tensor
auto weight_shape = ge::Shape({ 5,17,1,1 });
TensorDesc desc_weight_1(weight_shape, FORMAT_NCHW, DT_INT8);
Tensor weight_tensor(desc_weight_1);
uint32_t weight_1_len = weight_shape.GetShapeSize();
// "const_0.bin" 为常量文件的路径
GetConstTensorFromBin("const_0.bin", weight_tensor, weight_1_len*sizeof(int8_t));

// 创建Const算子，初始值（即属性value的值）为weight_tensor
auto conv_weight = op::Const("const_0").set_attr_value(weight_tensor);
```

GetConstTensorFromBin函数实现：

```c++
bool GetConstTensorFromBin(string path, Tensor &weight, uint32_t len) {
    // 以二进制模式打开文件
    ifstream in_file(path.c_str(), std::ios::in | std::ios::binary);
    if(!in_file.is_open()) {
        std::cout << "failed to open" << path.c_str() << "\n";
        return false;
    }
    // 将文件指针移动到文件末尾，获取文件总大小
    in_file.seekg(0,  std::ios_base::end);
    std::istream::pos_type file_size = in_file.tellg();
    in_file.seekg(0, ios_base::beg);  // 将文件指针移回文件开头

    // 检查用户指定的len是否与文件实际大小一致
    if(len != file_size) {
        std::cout << "Invalid Param.len:" << len << " is not equal with binary size(" << file_size << ")\n";
        in_file.close();
        return false;
    }
    // 为文件内容分配内存缓冲区
    char* pdata = new(std::nothrow) char[len];
    if(pdata == nullptr) {
        std::cout << "Invalid Param.len:" << len << " is not equal with binary size(" << file_size << ")\n";
        in_file.close();
        return false;
    }
    // 从文件中读取数据到缓冲区
    in_file.read(reinterpret_cast<char*>(pdata), len);
    // 将缓冲区中的数据设置到Tensor对象weight中
    auto status = weight.SetData(reinterpret_cast<uint8_t*>(pdata), len);
    if(status != ge::GRAPH_SUCCESS) {
        std::cout << "Set Tensor Data Failed"<< "\n";
        in_file.close();
        return false;
    }
    in_file.close();
    delete[] pdata;
    return true;
}
```

GetConstTensorFromBin函数参数说明：

- path：入参，指定权重文件路径，用于到固定目录例如“../data/weight/”下查找权重文件xx.bin，用户需要自行将权重文件解析为bin文件。
- weight：出参，从权重文件中读取的Tensor类型的权重数据。
- len：入参，指定权重数据大小。

## 定义必选输入算子（SoftmaxV2）

下面以SoftmaxV2为例，介绍如何进行算子定义。

SoftmaxV2算子原型定义：

```c++
REG_OP(SoftmaxV2)
    .INPUT(x, TensorType({ DT_DOUBLE, DT_FLOAT16, DT_BF16, DT_FLOAT }))
    .OUTPUT(y, TensorType({ DT_DOUBLE, DT_FLOAT16, DT_BF16, DT_FLOAT }))
    .ATTR(axes, ListInt, {-1})
    .ATTR(half_to_float, Bool, false)
    .OP_END_FACTORY_REG(SoftmaxV2)
```

从SoftmaxV2算子原型可以看到，SoftmaxV2算子有一个必选输入，输入名称为x。创建SoftmaxV2算子实例：

```c++
auto softmax = op::SoftmaxV2("Softmax")     // 创建算子实例，传算子名称（例如Softmax）作为入参
    .set_input_x(matmul2);                  // 设置算子输入（例如matmul2）
```

## 定义可选输入算子（Conv2D）

下面以Conv2D为例，介绍如何进行算子定义。

Conv2D算子原型定义：

```c++
REG_OP(Conv2D)
    .INPUT(x, TensorType({DT_FLOAT16, DT_FLOAT, DT_INT8, DT_BF16}))
    .INPUT(filter, TensorType({DT_FLOAT16, DT_FLOAT, DT_INT8, DT_BF16}))
    .OPTIONAL_INPUT(bias, TensorType({DT_FLOAT16, DT_FLOAT, DT_INT32}))
    .OPTIONAL_INPUT(offset_w, TensorType({DT_INT8}))
    .OUTPUT(y, TensorType({DT_FLOAT16, DT_FLOAT, DT_INT32, DT_BF16}))
    .REQUIRED_ATTR(strides, ListInt)
    .REQUIRED_ATTR(pads, ListInt)
    .ATTR(dilations, ListInt, {1, 1, 1, 1})
    .ATTR(groups, Int, 1)
    .ATTR(data_format, String, "NHWC")
    .ATTR(offset_x, Int, 0)
    .OP_END_FACTORY_REG(Conv2D)
```

从Conv2D算子原型定义可以看到，Conv2D算子包括：两个必选输入x和filter，两个可选输入bias和offset\_w，两个必选属性strides、pads，四个可选属性dilations、groups、data\_format、offset\_x。基于上述定义，Conv2D算子定义的代码为：

```c++
auto conv2d = op::Conv2D("Conv2d")
    // quant, conv_weight, conv_bias为三个输入节点
    .set_input_x(quant)
    .set_input_filter(conv_weight)
    .set_input_bias(conv_bias)
    .set_attr_strides({ 1, 1, 1, 1 })
    .set_attr_pads({ 0, 0, 0, 0 })
    .set_attr_dilations({ 1, 1, 1, 1 });

TensorDesc conv2d_input_desc_x(ge::Shape(), FORMAT_NCHW, DT_INT8);
TensorDesc conv2d_input_desc_filter(ge::Shape(), FORMAT_NCHW, DT_INT8);
TensorDesc conv2d_input_desc_bias(ge::Shape(), FORMAT_NCHW, DT_INT32);
TensorDesc conv2d_output_desc_y(ge::Shape(), FORMAT_NCHW, DT_INT32);
conv2d.update_input_desc_x(conv2d_input_desc_x);
conv2d.update_input_desc_filter(conv2d_input_desc_filter);
conv2d.update_input_desc_bias(conv2d_input_desc_bias);
conv2d.update_output_desc_y(conv2d_output_desc_y);
```

主要过程为：

1. 调用算子类型构造函数“Conv2D\(const char\* name\)”，创建算子实例并传入算子名称（例如Conv2d）作为入参。

    ```c++
    auto conv2d1 = op::Conv2D("Conv2d")
    ```

2. 调用“set\_input\__输入名称_”接口设置算子的输入。

    ```c++
        .set_input_x(data)
        .set_input_filter(conv_weight)
        .set_input_bias(conv_bias)
    ```

    data为整个graph的输入节点，通过Data算子构造，具体请参考[定义数据节点（Data）](#定义数据节点data)。

    conv\_weight为常量数据，通过Const算子构造，具体请参考[定义常量节点（Const）](#定义常量节点const)。

    conv\_bias为常量数据，通过Const算子构造，具体请参考[定义常量节点（Const）](#定义常量节点const)。

3. 调用“set\_attr\__属性名称_”接口设置算子的属性。

    ```c++
    .set_attr_strides({1, 1, 1, 1})       // 设置strides属性值
    .set_attr_pads({0, 0, 0, 0})          // 设置pads属性值
    .set_attr_dilations({1, 1, 1, 1});    // 设置dilations属性值
    ```

4. 对于Conv2D等卷积类或对C轴处理敏感的算子，建议通过“update\_input\_desc\_输入名称”接口将Format信息设置为NCHW或者NHWC等，具体和用户需要处理的Format格式保持一致。

    ```c++
    TensorDesc conv2d_input_desc_x(ge::Shape(), FORMAT_NCHW, DT_INT8);
    TensorDesc conv2d_input_desc_filter(ge::Shape(), FORMAT_NCHW, DT_INT8);
    TensorDesc conv2d_input_desc_bias(ge::Shape(), FORMAT_NCHW, DT_INT32);
    TensorDesc conv2d_output_desc_y(ge::Shape(), FORMAT_NCHW, DT_INT32);
    conv2d.update_input_desc_x(conv2d_input_desc_x);
    conv2d.update_input_desc_filter(conv2d_input_desc_filter);
    conv2d.update_input_desc_bias(conv2d_input_desc_bias);
    conv2d.update_output_desc_y(conv2d_output_desc_y);
    ```

    IR构图不支持输入以下FORMAT：

    ```c++
    FORMAT_NC1HWC0
    FORMAT_FRACTAL_Z
    FORMAT_NC1C0HWPAD
    FORMAT_NHWC1C0
    FORMAT_FRACTAL_DECONV
    FORMAT_C1HWNC0
    FORMAT_FRACTAL_DECONV_TRANSPOSE
    FORMAT_FRACTAL_DECONV_SP_STRIDE_TRANS
    FORMAT_NC1HWC0_C04
    FORMAT_FRACTAL_Z_C04
    FORMAT_FRACTAL_DECONV_SP_STRIDE8_TRANS
    FORMAT_NC1KHKWHWC0
    FORMAT_C1HWNCoC0
    FORMAT_FRACTAL_ZZ
    FORMAT_FRACTAL_NZ
    FORMAT_NDC1HWC0
    FORMAT_FRACTAL_Z_3D
    FORMAT_FRACTAL_Z_3D_TRANSPOSE
    FORMAT_FRACTAL_ZN_LSTM
    FORMAT_FRACTAL_Z_G
    FORMAT_ND_RNN_BIAS
    FORMAT_FRACTAL_ZN_RNN
    FORMAT_NYUV
    FORMAT_NYUV_A
    ```

## 定义动态多输入算子（AddN）

某些算子的输入个数不固定，为动态多输入算子，例如AddN，下面介绍如何定义这类算子。

AddN算子原型定义：

```c++
REG_OP(AddN)
    .DYNAMIC_INPUT(x, TensorType({NumberType(), DT_VARIANT}))
    .OUTPUT(y, TensorType({NumberType(), DT_VARIANT}))
    .REQUIRED_ATTR(N, Int)
    .OP_END_FACTORY_REG(AddN)
```

通过AddN算子原型定义可以看到，该算子为动态多输入算子；我们通过“create\_dynamic\_input\__输入名称_”创建动态输入，通过“set\_dynamic\_input\__输入名称_”设置动态输入。

```c++
auto data = op::Data().set_attr_index(0);
auto addn = op::AddN("addn")
 .create_dynamic_input_x(2)    // 创建动态输入x，包括2个输入，并且把这两个输入作为算子最后的输入
 .set_dynamic_input_x(0,data)     // 设置第1个输入，0表示输入索引，默认从0开始，data表示输入value
 .set_dynamic_input_x(1,data)     // 设置第2个输入，1表示输入索引，默认从0开始，data表示输入value
 .set_attr_N(2);                  // 设置属性N的值为2，表示该算子有2个输入
```

也可以通过“create\_dynamic\_input\_byindex\__输入名称_”创建动态输入，但是和“create\_dynamic\_input\__输入名称_”不能同时使用，两者的区别是：“create\_dynamic\_input\__输入名称_”默认把创建的动态输入作为算子最后的输入，而“create\_dynamic\_input\_byindex\__输入名称_”可以指定动态输入的索引位置，例如：

```c++
auto addn = op::AddN("addn")
 .create_dynamic_input_byindex_x(2,0)    // 创建动态输入x，包括2个输入，并将这两个输入插入到索引0和索引1的位置（其中0表示动态输入的起始索引）
 .set_dynamic_input_x(0,data1)                // 设置第1个输入，0表示输入索引，默认从0开始，data1表示输入value
 .set_dynamic_input_x(1,data2)                // 设置第2个输入，1表示输入索引，默认从0开始，data2表示输入value
 .set_attr_N(2);                              // 设置属性N的值为2，表示该算子有2个输入
```

## 定义动态多输出算子（Split）

某些算子的输出个数不固定，为动态多输出算子，例如Split，下面介绍如何定义这类算子。

Split算子原型定义：

```c++
REG_OP(Split)
    .INPUT(split_dim, TensorType({DT_INT32}))
    .INPUT(x, TensorType({DT_COMPLEX128, DT_COMPLEX64, DT_DOUBLE, DT_FLOAT,  DT_FLOAT16, DT_INT16,
                          DT_INT32,      DT_INT64,     DT_INT8,   DT_QINT16, DT_QINT32,  DT_QINT8,
                          DT_QUINT16,    DT_QUINT8,    DT_UINT16, DT_UINT32, DT_UINT64,  DT_UINT8,
                          DT_BF16,       DT_BOOL}))
    .DYNAMIC_OUTPUT(y, TensorType({DT_COMPLEX128, DT_COMPLEX64, DT_DOUBLE, DT_FLOAT,  DT_FLOAT16, DT_INT16,
                                   DT_INT32,      DT_INT64,     DT_INT8,   DT_QINT16, DT_QINT32,  DT_QINT8,
                                   DT_QUINT16,    DT_QUINT8,    DT_UINT16, DT_UINT32, DT_UINT64,  DT_UINT8,
                                   DT_BF16,       DT_BOOL}))
    .REQUIRED_ATTR(num_split, Int)
    .OP_END_FACTORY_REG(Split)

```

Split算子原型定义可以看到，该算子为动态多输出算子，我们通过“create\_dynamic\_output\__输出名称_”创建动态输出。

```c++
auto split = op::Split("split")
  .set_input_x(data)                         // 构造Data算子
  .set_input_split_dim(const)                // 构造Const算子
  .set_attr_num_split(2)
  .create_dynamic_output_y(2);           // 创建split算子的动态输出y，包括2个输出

auto addn = op::AddN("addn")
  .create_dynamic_input_x(1)                 // 创建动态输入x，包括1个输入
  .set_dynamic_input_x(0, split, "y0")       // 设置addn算子的第1个输入，split表示输入算子，“y0”表示split算子的输出名称， y0为第一个输出
  .set_attr_N(1);                            // 设置属性N的值为1，表示该算子有1个输入

auto softplus = op::Softplus("softplus")
  .set_input_x(split, "y1");                 // 设置softplus算子的输入，split表示输入算子，y1表示split算子的第2个输出
```

如何构造Data算子，请参考[定义数据节点（Data）](#定义数据节点data)，如何构造Const算子，请参考[定义常量节点（Const）](#定义常量节点const)。

## 定义数据类型转换算子（Cast）

通过算子原型构建Graph时，要求前后算子的dtype必须一致，上一个算子的输出dtype如果和下一层算子的输入dtype不匹配时需要插入Cast算子。

例如下面示例中，AddN算子要求输入float32，但是Greater算子的输出是bool类型，在数据类型发生变换的情况下，需要通过插入Cast算子进行数据类型转换。

![](../figures/define_dtype_convert_op.png)

Cast算子原型定义：

```c++
REG_OP(Cast)
    .INPUT(x, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8,
                          DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64,
                          DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32, DT_BF16, DT_UINT1}))
    .OUTPUT(y, TensorType({DT_BOOL, DT_FLOAT16, DT_FLOAT, DT_INT8, DT_INT32, DT_UINT32, DT_UINT8,
                           DT_INT64, DT_UINT64, DT_INT16, DT_UINT16, DT_DOUBLE, DT_COMPLEX64,
                           DT_COMPLEX128, DT_QINT8, DT_QUINT8, DT_QINT16, DT_QUINT16, DT_QINT32,
                           DT_BF16, DT_COMPLEX32}))
    .REQUIRED_ATTR(dst_type, Int)
    .OP_END_FACTORY_REG(Cast)
```

从Cast算子原型定义可以看到，有一个必选属性dst\_type，表示转换后的数据类型，设置为0表示转换后的数据类型为float32，值和数据类型对应关系请参见[《基础数据结构和接口》](https://hiascend.com/document/redirect/CannCommunitybasicopapi)中的"ge命名空间\>DataType"。

```c++
auto greater = op::Greater("greater").set_input_x1(const1).set_input_x2(const2);
auto cast = op::Cast("cast").set_input_x(greater)
                            .set_attr_dst_type(0);
auto addn = op::AddN("addn").create_dynamic_input_x(3)
                           .set_dynamic_input_x(0,cast)
                           .set_dynamic_input_x(1,data).set_dynamic_input_x(2,data)
                           .set_attr_N(3);
```
