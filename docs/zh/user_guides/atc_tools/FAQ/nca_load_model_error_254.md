# 系统报错"nca load model from file failed, ret = 254."

## 问题现象

量化过程中，如果出现如下日志，表示可能没有获取到分组信息。

```console
[INFO][ncx_interface.cpp:35] ncx lib has been init
[INFO][ncx_interface.cpp:35] ncx lib has been init
[ERROR][ncx_interface.cpp:208] nca load model from file failed, ret = 254. please check detail info from device log.
```

根据上述ERROR信息，查看“/home/mdc/var/log/debug/TOOLS\_\*.log”，进一步定位，如果出现如下信息，可以确定该错误原因是：NCS没有获取到分组信息而创建分组失败。

```console
[INFO][ncx_interface.cpp:35] ncx lib has been init
[INFO][ncx_interface.cpp:35] ncx lib has been init
[ERROR][ncx_interface.cpp:208] nca load model from file failed, ret = 254. please check detail info from device log.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[sample_process.cpp:378]NCX load model failed.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[sample_process.cpp:442]Ncx execute failed.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[sample_process.cpp:297]Ncx model infer failed.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[quantize_api.cpp:268]sample process failed
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[quantize_api.cpp:314]Calibration Inference failed!
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[quantize_api.cpp:439]Do Calibration failed.
[ERROR][ncx_interface.cpp:168] nca close session: amct ncx lib has not been init
[ERROR][ncx_interface.cpp:135] nca finalize: amct ncx lib has not been init
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[sample_process.cpp:518]NCX finalize failed.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[sample_process.cpp:460]NCX destroy resource failed.
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[quantize_api.cpp:184]sample process destroy resource failed!
[ERROR] AMCT(1372344,acl_test_tool):2025-09-12-08:26:47[inner_graph_calibration.cpp:73]Failed to finalize CalibrationInitializer.
```

## 解决方案

确保已经创建设置分组，创建分组的方法为：

调用DCMI提供的dcmi\_create\_capability\_group接口创建分组。
