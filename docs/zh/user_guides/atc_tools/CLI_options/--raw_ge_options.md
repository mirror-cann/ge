# --raw\_ge\_options

## 产品支持情况

全量芯片支持

## 功能说明

指定包含GE编译选项的JSON文件路径。ATC工具会解析该JSON文件中的"compile options"字段，并将其转换为GE编译所需的具体参数。

适用场景：当用户使用[options参数](../../../api/graph_engine_api/cpp/ge/options_params/overall_constraints.md)进行了在线或者离线推理，并希望将这些配置同步应用于ATC工具的离线推理流程，则可通过指定此JSON文件实现配置复用，确保两端推理行为一致。

## 关联参数

若--raw\_ge\_options指定的JSON文件中包含ATC工具不支持的选项，需配合[--raw\_ge\_options\_ignore\_unsupported](--raw_ge_options_ignore_unsupported.md)参数使用，当该参数设置为true时，ATC工具将忽略不支持的选项并打印warning，继续处理其余选项；当设置为false时，遇到不支持的选项将直接报错终止。

## 参数取值

**参数值：**
JSON文件的路径和文件名。JSON文件示例如下：

```json
{
  "compile options": {
    "global": {
      "ge.jit_compile": "1"
    },
    "session": {
      "ge.jit_compile": "1"
    },
    "graph": {
      "ge.jit_compile": "2"
    }
  },
  "execute options": {
    "graph": {
      "ge.exec.staticMemoryPolicy": "2"
    }
  }
}
```

ATC工具仅解析其中的“compile options”，"execute options"永远忽略，并将其中的global、session、graph三个对象进行扁平化合并，合并优先级为：graph \> session \> global；

对于相同的参数，生效优先级为：ATC命令行显式设置的参数 \> --raw\_ge\_options JSON文件中的参数 \> ATC参数默认值。

例如上述示例中的ge.jit\_compile参数（参数解释请参见[options参数说明](../../../api/graph_engine_api/cpp/ge/options_params/overall_constraints.md)），JSON文件最终取值为2，如果通过ATC命令行同时输入[--jit\_compile](--jit_compile.md)=0，则最终生效的为0。

**参数值约束：**
JSON文件中的每个options key必须非空。

## 推荐配置及收益

无。

## 示例

将上述JSON文件上传到ATC工具所在服务器路径，执行如下命令：

```bash
atc --raw_ge_options=$HOME/module/option.json --model=model.onnx ...
```

## 依赖约束

无。
