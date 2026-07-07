# SetStreamId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

设置Task的流id。

## 函数原型

```c++
void SetStreamId(uint32_t stream_id)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| stream_id | 输入 | 流id。 |

## 返回值说明

无

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  auto wait_task = KernelLaunchInfo::CreateHcomWaitTask(context);
  wait_task.SetStreamId(attach_stream_id);
  ...
}
```
