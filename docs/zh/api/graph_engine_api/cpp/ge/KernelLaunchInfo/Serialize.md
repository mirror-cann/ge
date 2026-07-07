# Serialize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

将KernelLaunchInfo序列化成数据流。

## 函数原型

```c++
std::vector<uint8_t> Serialize()
```

## 参数说明

无

## 返回值说明

返回序列化后的数据流。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  // 创建WaitTask
  auto wait_task = KernelLaunchInfo::CreateHcomWaitTask(context);
  wait_task.SetStreamId(attach_stream_id);
  // 序列化
  tasks.insert(tasks.begin() + aicore_index, wait_task.Serialize());
  aicore_index++;
  ...
}
```
