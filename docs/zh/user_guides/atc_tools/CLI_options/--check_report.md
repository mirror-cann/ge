# --check\_report

## 产品支持情况

<!-- npu="950,A3,910b,910,310p,310b" id2 -->
全量芯片支持
<!-- end id2 -->

<!-- @ref: ge/res/docs/zh/user_guides/atc_tools/CLI_options/--check_report_res.md#id1 -->

<!-- npu="IPV350" id1 -->
IPV350：不支持
<!-- end id1 -->

## 功能说明

用于配置预检结果保存文件路径和文件名。

## 关联参数

[--mode](--mode.md)：当--mode=0时解析图失败时或--mode=3仅做预检时，通过该参数指定预检结果文件的保存路径。

## 参数取值

**参数值：**
预检结果文件路径和文件名。

**参数值格式：**
路径和文件名：支持大小写字母（a-z，A-Z）、数字（0-9）、下划线（\_）、短横线（-）、句点（.）、中文汉字。

**参数默认值：**
执行atc命令当前路径生成check\_result.json

## 推荐配置及收益

无。

## 示例

```bash
atc --check_report=$HOME/module/out/check_result.json ...
```

## 使用约束

预检结果文件存储路径，除[--check\_report](--check_report.md)参数设置的方式外，还可以配置环境变量ASCEND\_WORK\_PATH，几种方式优先级为：配置参数“--check\_report”\>环境变量ASCEND\_WORK\_PATH\>默认存储路径（执行atc命令当前路径）。

关于环境变量ASCEND\_WORK\_PATH的详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。
