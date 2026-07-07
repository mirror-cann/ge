# CreateHiddenInput

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

创建一个[kHiddenInput](../ArgDescType.md)类型（在IR原型定义上不存在的输入地址）的ArgDescInfo对象，表示此块Args地址为HiddenInput地址。

## 函数原型

```c++
static ArgDescInfo CreateHiddenInput(HiddenInputSubType hidden_type)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| hidden_type | 输入 | Hidden输入的的类型，具体类型请参见[HiddenInputSubType](../HiddenInputSubType.md)。 |

## 返回值说明

返回一个ArgDescInfo对象，此对象的type为kHiddenInput。

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
  // 创建一个类型是Hcom的HiddenInput地址
  aicpu_args_format.emplace_back(ArgDescInfo::CreateHiddenInput(HiddenInputSubType::kHcom));
...
}
```
