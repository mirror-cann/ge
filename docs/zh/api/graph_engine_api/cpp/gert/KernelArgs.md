# KernelArgs

用于描述kernel launch参数的结构体，包含参数数据指针、大小和内存位置信息。主要用于自定义算子的args内存管理和地址刷新场景。

头文件位于CANN软件安装后文件存储路径下的include/exe\_graph/runtime/kernel\_args.h。

```c++
struct KernelArgs {
  void *args_data;      ///< Pointer to argument data
  size_t args_size;     ///< Size of argument data in bytes
  uint64_t reserved[4]; ///< Reserved for future extension
  Placement placement;  ///< Memory placement (host or device)
  uint8_t placement_reserved_[4]; ///< Reserved field, 4-byte aligned for Placement
  KernelArgs()
      : args_data(nullptr), args_size(0U), reserved{},
        placement(ge::kPlacementHost), placement_reserved_{} {}
};
```

- args\_data：参数数据指针，指向host或device上的参数buffer。
- args\_size：参数数据大小（字节）
- reserved\[4\]：预留字段，用于未来扩展。
- placement：内存位置标识，枚举类型。
  - ge::kPlacementHost：Host内存（主机侧）。
  - ge::kPlacementDevice：Device内存（设备侧）。

- placement\_reserved\_\[4\]：预留字段，4字节对齐。
