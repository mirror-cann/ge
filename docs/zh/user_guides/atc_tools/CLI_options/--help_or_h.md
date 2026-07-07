# --help或--h

## 产品支持情况

全量芯片支持

## 功能说明

显示帮助信息。

## 关联参数

无。

## 参数取值

无。

## 推荐配置及收益

无。

## 示例

```bash
atc --help
```

返回的部分信息如下所示：

```console
ATC start working now, please wait for a moment.
usage: atc <args>
generate offline model example:
atc --model=./alexnet.prototxt --weight=./alexnet.caffemodel --framework=0 --output=./domi --soc_version=<soc_version>
generate offline model for single op example:
atc --singleop=./op_list.json --output=./op_model --soc_version=<soc_version>

===== Basic Functionality =====
[General]
  --h/help            Show this help message
... ...
```

## 依赖约束

无。
