# --raw\_ge\_options\_ignore\_unsupported

## 产品支持情况

全量芯片支持

## 功能说明

控制[--raw\_ge\_options](--raw_ge_options.md)参数指向的JSON文件中包含ATC工具不支持的GE option时，ATC工具的处理行为。

## 关联参数

仅在[--raw\_ge\_options](--raw_ge_options.md)非空时生效。

## 参数取值

**参数值：**

- true：ATC工具将忽略不支持的选项并打印warning，继续处理其余选项；
- false（默认值）：遇到不支持的选项将直接报错终止。

**参数值约束：**
JSON文件中的每个options key必须非空。

## 推荐配置及收益

无。

## 示例

```bash
atc --raw_ge_options=$HOME/module/option.json --raw_ge_options_ignore_unsupported=false --model=model.onnx ...
```

## 依赖约束

无。
