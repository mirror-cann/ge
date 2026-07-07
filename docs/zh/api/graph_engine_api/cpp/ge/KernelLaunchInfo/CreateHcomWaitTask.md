# CreateHcomWaitTask

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

创建一个Wait Task，此Task用于阻塞流，当与其有相同group\_name的Record Task被执行时，阻塞会被解除。

## 函数原型

```c++
static KernelLaunchInfo CreateHcomWaitTask(const gert::ExeResGenerationContext *context, const char *group_name = "group")
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| context | 输入 | GenerateTask函数的入参，保存了算子的基础信息。 |
| group_name | 输入 | Wait Task的分组名字，默认为group，用于与Record Task配套。 |

## 返回值说明

返回创建出来的Wait Task信息。

## 约束说明

group\_name必须与算子原型中定义的属性一致。例如，某个mc2算子定义了一个属性group\_ep，则可以使用group\_name为group\_ep创建Record任务和Wait任务。

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  // 创建WaitTask
  auto wait_task = KernelLaunchInfo::CreateHcomWaitTask(context);
  wait_task.SetStreamId(attach_stream_id);
  tasks.insert(tasks.begin() + aicore_index, wait_task.Serialize());
  aicore_index++;
  ...
}
```
