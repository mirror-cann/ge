# GetIrIndex

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

获取当前ArgDescInfo的算子IR索引。只有当type为kIrInput、kIrOutput、kIrInputDesc、kIrOutputDesc时才可以获取到。

## 函数原型

```c++
int32_t GetIrIndex() const
```

## 参数说明

无

## 返回值说明

当前Args地址对应输入/输出的IR索引，未设置时默认值为-1。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
...
  // 设置AI CPU任务
  auto aicpu_task = KernelLaunchInfo::CreateAicpuKfcTask(context,
      "libccl_kernel.so", "RunAicpuKfcSrvLaunch");
  std::vector<ArgDescInfo> aicpu_args_format;
  aicpu_args_format.emplace_back(ArgDescInfo(ArgDescType::kIrOutput, 0));
  // 获取kIrOutput类型的ArgDescInfo的IR索引,此时获取到的值为0
  auto ir_index = aicpu_args_format.back().GetIrIndex();
...
}
```
