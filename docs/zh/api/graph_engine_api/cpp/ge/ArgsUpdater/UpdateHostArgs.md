# UpdateHostArgs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：liblowering.so

## 功能说明

更新算子的host args buffer。

## 函数原型

```c++
virtual graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) = 0
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| UpdateArgsContext | 输入 | 可通过该上下文获取args buffer和更新后的I/O地址。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | graphStatus | GRAPH_SUCCESS(0)：成功<br>其他值：失败 |

## 约束说明

无

## 调用示例

```c++
graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
  // 获取输入输出张量的最新地址
  const gert::Tensor *input_x = ctx->GetInputTensor(kInputX);
  const gert::Tensor *input_y = ctx->GetInputTensor(kInputY);
  const gert::Tensor *output_z = ctx->GetOutputTensor(kOutputZ);

  if (input_x == nullptr || input_y == nullptr || output_z == nullptr) {
    LOG_ERROR("UpdateHostArgs failed, input_x=", input_x, " input_y=", input_y, " output_z=", output_z);
    return GRAPH_FAILED;
  }

  // 获取host侧args指针
  const gert::KernelArgs *host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);
  if (host_args == nullptr) {
    LOG_ERROR("UpdateHostArgs failed, host_args is null");
    return GRAPH_FAILED;
  }

  // 仅刷新args中的地址字段
  auto *args = static_cast<AddArgs *>(host_args->args_data);
  args->x_ptr = input_x->GetAddr();
  args->y_ptr = input_y->GetAddr();
  args->z_ptr = const_cast<void *>(output_z->GetAddr());

  return GRAPH_SUCCESS;
}
```
