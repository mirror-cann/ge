# --is\_output\_adjust\_hw\_layout

## 产品支持情况

全量芯片支持

## 功能说明

与[--out\_nodes](--out_nodes.md)参数配合使用，指定网络模型输出数据类型为float16、数据格式为NC1HWC0。单独配置本参数无效，但模型转换成功。

## 关联参数

- --is\_output\_adjust\_hw\_layout参数设置为true，需要与[--out\_nodes](--out_nodes.md)参数配合使用，通过--out\_nodes参数指定的节点输出数据类型为float16、输出数据格式为NC1HWC0。

    --is\_output\_adjust\_hw\_layout参数取值个数必须和--out\_nodes参数指定节点个数匹配，多个参数取值使用英文逗号分隔。

- 当[--framework](--framework.md)取值为1，且为MindSpore框架网络模型时，设置本参数无效，但模型转换成功。

## 参数取值

- true：指定网络模型输出数据类型为float16、数据格式为NC1HWC0。
- false：（默认值）不指定。

## 推荐配置及收益

无。

## 示例

- 若--out\_nodes配置了一个节点

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow.pb --framework=3 --output=$HOME/module/out/tf_resnet50 --is_output_adjust_hw_layout=true  --out_nodes="prob:0" --soc_version=<soc_version> ...
    ```

- 若--out\_nodes配置了多个节点

    ```bash
    atc --model=$HOME/module/resnet50_tensorflow.pb --framework=3 --output=$HOME/module/out/tf_resnet50 --is_output_adjust_hw_layout="true,true"  --out_nodes="prob:0;prob:1" --soc_version=<soc_version> ...
    ```

## 依赖约束

无。
