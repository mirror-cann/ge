# CreateAicpuKfcTask

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

创建一个AI CPU的KFC Task。

## 函数原型

```c++
static KernelLaunchInfo CreateAicpuKfcTask(const gert::ExeResGenerationContext *context, const char *so_name, const char *kernel_name)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| context | 输入 | GenerateTask函数的入参，保存了算子的基础信息。 |
| so_name | 输入 | AI CPU算子打包的so名称。 |
| kernel_name | 输入 | AI CPU算子的入口函数名称。 |

## 返回值说明

返回创建出的AI CPU KFC Task信息。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  // 创建AI CPU任务
  auto aicpu_task = KernelLaunchInfo::CreateAicpuKfcTask(context,
      "libccl_kernel.so", "RunAicpuKfcSrvLaunch");
  // 获取attach流
  auto stream_infos = context->GetAttachedStreamInfos();
  GE_ASSERT_TRUE(!stream_infos.empty());
  const int64_t attach_stream_id = stream_infos[0].stream_id;
  // 往AI CPU KFC的任务中添加信息
  aicpu_task->SetStreamId(attach_stream_id);
  ...
}
```
