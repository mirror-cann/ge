# --dump\_mode

## 产品支持情况

全量芯片支持

## 功能说明

是否生成带shape信息的JSON文件。

## 关联参数

- 该参数需要与[--json](--json.md)、[--mode](--mode.md)=1、[--framework](--framework.md)、[--om](--om.md)参数（需要为原始模型文件，如果为Caffe框架模型文件，还需要增加[--weight](--weight.md)参数）配合使用。

## 参数取值

- 0：（默认值）生成不带shape信息的JSON文件。
- 1：生成带shape信息的JSON文件。

## 推荐配置及收益

无。

## 示例

```bash
atc --mode=1 --om=$HOME/module/resnet50_tensorflow.pb --json=$HOME/module/out/tf_resnet50.json --framework=3 --dump_mode=1 ...
```

## 依赖约束

无。
