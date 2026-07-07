# GetKernelArgs

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/custom\_op.h\>
- 库文件：liblowering.so

## 功能说明

获取指定位置和索引的kernel args。

## 函数原型

```c++
const KernelArgs* GetKernelArgs(Placement placement, size_t index) const
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| placement | 输入 | kPlacementHost获取host args，kPlacementDevice获取device args。 |
| index | 输入 | args索引，与MallocReadOnlyDevArgs的调用顺序对应（首次malloc对应index 0，后续依次递增）。 |

## 返回值说明

KernelArgs指针。

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
