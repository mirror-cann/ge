# 图编译缓存

图编译缓存功能支持将图编译结果进行磁盘持久化，从而在应用程序重新运行时可直接加载磁盘上缓存的编译结果。在**编译并运行Graph**过程中，为了减少图编译时长，可以开启图编译缓存功能。

<!-- npu="310b" id1 -->
Atlas 200I/500 A2 推理产品不支持该特性。
<!-- end id1 -->

## 使用约束

- 根据options参数中的ge.graph\_compiler\_cache\_dir和ge.graph\_key确定缓存文件，缓存文件不存在则保存缓存，缓存文件存在则直接加载缓存。ge.graph\_compiler\_cache\_dir和ge.graph\_key同时配置非空时该功能生效。
- ge.graph\_compiler\_cache\_dir指定的缓存目录必须存在，否则会导致编译失败。
- ge.graph\_key由用户保证其唯一性，否则，在相同ge.graph\_key的缓存已存在的情况下，会直接加载对应的缓存。
- 多进程场景下如果不同的图使用了相同的graph key和cache dir，后触发缓存的图将不会实际编译，而是加载先缓存的图。
- 图发生变化后，原来的缓存文件不可用，用户需要手动删除缓存目录中的缓存文件或者修改ge.graph\_key，重新编译生成缓存文件。
- 跨版本的缓存无法保证兼容性，如果版本升级，需要清理缓存目录重新编译生成缓存。

## 使用方法

```c++
std::map<ge::AscendString, ge::AscendString> session_options = {{"ge.graph_compiler_cache_dir", "./build_cache_dir"}};
std::shared_ptr<ge::GeSession> session = std::make_shared<ge::GeSession>(session_options);
const auto graph = CreateGraph();
std::map<ge::AscendString, ge::AscendString> graph_options = {{"ge.graph_key", "test_graph_001"}};
ge::Status ret = session->AddGraph(0, graph, graph_options);
...
```

## 缓存文件生成规则

生成文件包括：

- 模型缓存文件
- 索引文件，便于用户通过graph\_key快速找到对应的缓存文件，索引文件内容示例如下：

    ```json
    {
        "cache_file_list":[
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117202307.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117202307.rdcpkt"
            },
            {
                "cache_file_name":"./cache_dir/graph_$key1_20230117203007.om",
                "graph_key":"graph_$key1",
                "var_desc_file_name":"./cache_dir/graph_$key1_20230117203007.rdcpkt"
            }
        ]
    }
    ```

- 变量格式文件，仅在图中存在变量时生成。用于框架匹配模型缓存文件，如果graph\_key对应的图内变量格式发生变更，则之前缓存的缓存文件将无法直接恢复使用，该场景下会重新触发编译流程重新生成缓存文件。
- 如果开启了权重外置功能，即options选项中配置了ge.externalWeight参数，并且设置为1，则在**ge.graph\_compiler\_cache\_dir**参数指定路径下还会生成weight目录，用于保存原始网络中的Const/Constant节点的权重信息。

文件名生成规则：

- 索引文件命名为：ge.graph\_key + “.idx”。
- 模型缓存文件命名为: ge.graph\_key + 时间戳 + “.om”。
- 变量格式文件命名为: ge.graph\_key + 时间戳 + “.rdcpkt”。
