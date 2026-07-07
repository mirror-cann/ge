# aclmdlExecConfigAttr

```c
typedef enum {
    ACL_MDL_STREAM_SYNC_TIMEOUT = 0,
    ACL_MDL_EVENT_SYNC_TIMEOUT,
    ACL_MDL_WORK_ADDR_PTR,
    ACL_MDL_WORK_SIZET,
    ACL_MDL_MPAIMID_SIZET,      /** param reserved */
    ACL_MDL_AICQOS_SIZET,       /** param reserved */
    ACL_MDL_AICOST_SIZET,       /** param reserved */
    ACL_MDL_MEC_TIMETHR_SIZET   /** param reserved */
} aclmdlExecConfigAttr;
```

**表 1**  枚举项说明

| 枚举项 | 说明 |
| --- | --- |
| ACL_MDL_STREAM_SYNC_TIMEOUT | 在执行模型推理时控制Stream任务的超时时间。该属性值为INT32类型。<br>取值说明如下：<br><br>  - -1：表示永久等待，默认永久等待。<br>  - >0：配置具体的超时时间，单位是毫秒。 |
| ACL_MDL_EVENT_SYNC_TIMEOUT | 在执行模型推理时控制Event任务的超时时间。该属性值为INT32类型。<br>取值说明如下：<br><br>  - -1：表示永久等待，默认永久等待。<br>  - >0：配置具体的超时时间，单位是毫秒。 |
| ACL_MDL_WORK_ADDR_PTR | 模型所需工作内存（Device上存放模型执行过程中的临时数据）的指针，由用户管理工作内存。一般用于模型一次加载、多并发执行的场景。<br>如果同时配置ACL_MDL_WORK_ADDR_PTR以及aclrtStreamConfigAttr中的ACL_RT_STREAM_WORK_ADDR_PTR（表示Stream上模型的工作内存），则以ACL_MDL_WORK_ADDR_PTR优先。 |
| ACL_MDL_WORK_SIZET | 模型所需工作内存的大小，单位为Byte。一般用于模型一次加载、多并发执行的场景。 |
| ACL_MDL_MPAIMID_SIZET | 预留配置。 |
| ACL_MDL_AICQOS_SIZET | 预留配置。 |
| ACL_MDL_AICOST_SIZET | 预留配置。 |
| ACL_MDL_MEC_TIMETHR_SIZET | 预留配置。 |

<!-- npu="950,A3,910b,910,310p,310b" id1 -->
注意，当前仅支持ACL\_MDL\_STREAM\_SYNC\_TIMEOUT和ACL\_MDL\_EVENT\_SYNC\_TIMEOUT。
<!-- end id1 -->

<!-- npu="IPV350" id2 -->
注意，当前仅支持ACL\_MDL\_WORK\_ADDR\_PTR和ACL\_MDL\_WORK\_SIZET。
<!-- end id2 -->
