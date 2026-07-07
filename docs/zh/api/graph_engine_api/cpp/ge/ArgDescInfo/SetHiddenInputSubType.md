# SetHiddenInputSubType

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 功能说明

设置ArgDescInfo的隐藏输入地址的类型，只有type为kHiddenInput时，才能设置成功。

## 函数原型

```c++
graphStatus SetHiddenInputSubType(HiddenInputSubType hidden_type)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| hidden_type | 输入 | 隐藏输入的类型。 |

## 返回值说明

设置成功时返回“ge::GRAPH\_SUCCESS”。

关于graphStatus的定义，请参见[ge::graphStatus](https://hiascend.com/document/redirect/CannCommunitybasicopapi)。

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
  ArgDescInfo args_info(ArgDescType::kHiddenInput);
  args_info.SetHiddenInputSubType(HiddenInputSubType::kHcom);
  aicpu_args_format.emplace_back(args_info);
...
}
```
