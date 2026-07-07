# ArgDescInfo构造函数和析构函数

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

ArgDescInfo构造函数和析构函数。

## 函数原型

```c++
explicit ArgDescInfo(ArgDescType arg_type, int32_t ir_index = -1, bool is_folded = false)
~ArgDescInfo()
ArgDescInfo(const ArgDescInfo &other)
ArgDescInfo(ArgDescInfo &&other) noexcept
ArgDescInfo &operator=(const ArgDescInfo &other)
ArgDescInfo &operator=(ArgDescInfo &&other) noexcept
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| arg_type | 输入 | 当前Args地址的类型。具体类型定义请参考[ArgDescType](../ArgDescType.md)。 |
| ir_index | 输入 | 当前Args地址对应的算子IR索引。 |
| is_folded | 输入 | 当前地址是否需要被折叠成二级指针。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context, std::vector<std::vector<uint8_t>> &tasks) {
...
  // 设置AI CPU任务
  auto aicpu_task = KernelLaunchInfo::CreateAicpuKfcTask(context, "libccl_kernel.so", "RunAicpuKfcSrvLaunch");
  std::vector<ArgDescInfo> aicpu_args_format;
  // 构造了一个类型为kIrOutputDesc，ir_index为0，需要被折叠成二级指针的地址描述信息
  aicpu_args_format.emplace_back(ArgDescInfo(ArgDescType::kIrOutputDesc, 0, true));
...
}
```
