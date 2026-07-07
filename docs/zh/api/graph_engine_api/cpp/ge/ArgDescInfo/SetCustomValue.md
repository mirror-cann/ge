# SetCustomValue

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

设置ArgDescInfo的自定义值，只有type为kCustomValue时，才能设置成功。

## 函数原型

```c++
graphStatus SetCustomValue(uint64_t custom_value)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| custom_value | 输入 | 自定义值。 |

## 返回值说明

设置成功时返回“ge::GRAPH\_SUCCESS”。

关于graphStatus的定义，请参见[ge::graphStatus](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。

## 约束说明

无

## 调用示例

```c++
// 需要存储在Args中的结构体
struct HcclCommParamDesc {
  uint64_t version : 4;
  uint64_t group_num : 4;
  uint64_t has_ffts : 1;
  uint64_t tiling_off : 7;
  uint64_t is_dyn : 48;
};

graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
...
  // 设置AI CPU任务
  auto aicpu_task = KernelLaunchInfo::CreateAicpuKfcTask(context,
      "libccl_kernel.so", "RunAicpuKfcSrvLaunch");
  size_t input_size = context->GetComputeNodeInfo()->GetIrInputsNum();
  size_t output_size = context->GetComputeNodeInfo()->GetIrOutputsNum();
  const size_t offset = 3UL;
  union {
    HcclCommParamDesc hccl_desc;
    uint64_t custom_value;
  } desc;
  // 赋值
  desc.hccl_desc.version = 1;
  desc.hccl_desc.group_num = 1;
  desc.hccl_desc.has_ffts = 0;
  desc.hccl_desc.tiling_off = offset + input_size + output_size;
  desc.hccl_desc.is_dyn = 0;
  std::vector<ArgDescInfo> aicpu_args_format;
  ArgDescInfo custom_value_arg(ArgDescType::kCustomValue);
  // 设置自定义值
  custom_value_arg.SetCustomValue(desc.custom_value);
  // 将此结构体的内容转化成uint64_t的数字保存到ArgsFormat中
  aicpu_args_format.emplace_back(custom_value_arg)
...
}
```
