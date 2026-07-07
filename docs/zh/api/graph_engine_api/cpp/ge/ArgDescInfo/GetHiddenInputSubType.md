# GetHiddenInputSubType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

获取ArgDescInfo的隐藏输入地址的类型，只有type为kHiddenInput时，才能获取到正确的值。

## 函数原型

```c++
HiddenInputSubType GetHiddenInputSubType() const
```

## 参数说明

无

## 返回值说明

获取到隐藏输入地址的类型，默认值为kEnd。

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
  aicpu_args_format.emplace_back(ArgDescInfo::CreateHiddenInput(HiddenInputSubType::kHcom));
  // 此处获取到kHcom
  uint64_t hidden_type = aicpu_args_format.back().GetHiddenInputSubType();
...
}
```
