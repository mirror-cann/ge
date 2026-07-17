# MAX\_RUNTIME\_CORE\_NUMBER

## 功能描述

训练与在线推理场景下，针对动态shape图模式执行的网络，可通过设置此环境变量开启图执行器（Host侧）的多线程任务调度。

该环境变量需要配置为整数，当取值大于等于2时，表示Host侧图执行调度任务开启多线程，多线程数量与此环境变量取值相同。

若使用该环境变量，建议配置为3，以获得较优性能。

## 配置示例

```bash
export MAX_RUNTIME_CORE_NUMBER=3
```

## 使用约束

1. 此环境变量仅用于图模式场景。
2. 配置此环境变量时，Host侧可用于执行的CPU核数需要大于等于2。
3. 图执行时，需要在执行第一次迭代前将Host进程绑定到指定的CPU核上，以获得更优性能。

    下面给出PyTorch图模式场景下的绑核示例，假设Host侧共有192个CPU核，8个进程，每个进程绑定24个CPU核，代码片段如下：

    ```bash
    import os
    import psutil
    import torch
    # 绑核
    cpu_ids_array = [range(i*24, (i+1)*24) for i in range(0,8)]
    rank_os_par_array = [f"kernel_bond_rank{i}" for i in range(0,8)]
    rank_no = int(torch.distributed.get_rank())
    kernel_bond_os_par = int(os.getenv(rank_os_par_array[rank_no], "0"))
    if kernel_bond_os_par == 0:
       logging.info(f"rank{rank_no} is about to bound cpu kernels")
       pid = os.getpid()
       process = psutil.Process(pid)
       process.cpu_affinity(cpu_ids_array[rank_no])
       os.environ[rank_os_par_array[rank_no]] = str(kernel_bond_os_par + 1)
    ```

## 产品支持情况

<!-- npu="950" id1 -->
Ascend 950PR/Ascend 950DT：支持
<!-- end id1 -->

<!-- npu="A3" id2 -->
Atlas A3 训练系列产品/Atlas A3 推理系列产品：支持
<!-- end id2 -->

<!-- npu="910b" id3 -->
Atlas A2 训练系列产品/Atlas A2 推理系列产品：支持
<!-- end id3 -->

<!-- npu="310b" id6 -->
Atlas 200I/500 A2推理产品：不支持
<!-- end id6 -->

<!-- npu="310p" id4 -->
Atlas 推理系列产品：支持
<!-- end id4 -->

<!-- npu="910" id5 -->
Atlas 训练系列产品：支持
<!-- end id5 -->
