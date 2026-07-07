# 获取xlacompile.patch补丁文件

用户安装完xlacompile.patch补丁，编译成xlacompile工具后，该工具可以将有控制流的V1网络模型转成函数类的V2网络模型。

将如下代码复制到文件中，并另存为xlacompile.patch，然后上传到Linux服务器tensorflow-1.15.0路径下：

```diff
---
 WORKSPACE                                      |   7 +
 tensorflow/compiler/aot/BUILD                  |  27 ++++
 tensorflow/compiler/aot/xlacompile_main.cc     | 170 +++++++++++++++++++++
 tensorflow/compiler/tf2xla/tf2xla.cc           |   6 +
 tensorflow/compiler/tf2xla/tf2xla.h            |   4 +
 5 files changed, 195 insertions(+)
 create mode 100644 tensorflow/compiler/aot/xlacompile_main.cc

diff --git a/WORKSPACE b/WORKSPACE
index 74ea14d..d2265f9 100644
--- a/WORKSPACE
+++ b/WORKSPACE
@@ -34,6 +34,13 @@ load(

 bazel_toolchains_repositories()

+http_archive(
+    name = "io_bazel_rules_docker",
+    sha256 = "6287241e033d247e9da5ff705dd6ef526bac39ae82f3d17de1b69f8cb313f9cd",
+    strip_prefix = "rules_docker-0.14.3",
+    urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.14.3/rules_docker-v0.14.3.tar.gz"],
+)
+
 load(
     "@io_bazel_rules_docker//repositories:repositories.bzl",
     container_repositories = "repositories",

diff --git a/tensorflow/compiler/aot/BUILD b/tensorflow/compiler/aot/BUILD
index f871115..b2620db 100644
--- a/tensorflow/compiler/aot/BUILD
+++ b/tensorflow/compiler/aot/BUILD
@@ -106,6 +106,33 @@ cc_library(
     ],
 )

+tf_cc_binary(
+    name = "xlacompile",
+    visibility = ["//visibility:public"],
+    deps = [":xlacompile_main"],
+)
+
+cc_library(
+    name = "xlacompile_main",
+    srcs = ["xlacompile_main.cc"],
+    visibility = ["//visibility:public"],
+    deps = [
+        ":tfcompile_lib",
+        "//tensorflow/compiler/tf2xla:tf2xla_proto",
+        "//tensorflow/compiler/tf2xla:tf2xla_util",
+        "//tensorflow/compiler/xla:debug_options_flags",
+        "//tensorflow/compiler/xla/service:compiler",
+        "//tensorflow/core:core_cpu",
+        "//tensorflow/core:core_cpu_internal",
+        "//tensorflow/core:framework",
+        "//tensorflow/core:framework_internal",
+        "//tensorflow/core:graph",
+        "//tensorflow/core:lib",
+        "//tensorflow/core:protos_all_cc",
+        "@com_google_absl//absl/strings",
+    ],
+)
+
 # NOTE: Most end-to-end tests are in the "tests" subdirectory, to ensure that
 # tfcompile.bzl correctly handles usage from outside of the package that it is
 # defined in.

diff --git a/tensorflow/compiler/aot/xlacompile_main.cc b/tensorflow/compiler/aot/xlacompile_main.cc
new file mode 100644
index 0000000..bc795ef
--- /dev/null
+++ b/tensorflow/compiler/aot/xlacompile_main.cc
@@ -0,0 +1,170 @@
+/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.
+
+Licensed under the Apache License, Version 2.0 (the "License");
+you may not use this file except in compliance with the License.
+You may obtain a copy of the License at
+
+    http://www.apache.org/licenses/LICENSE-2.0
+
+Unless required by applicable law or agreed to in writing, software
+distributed under the License is distributed on an "AS IS" BASIS,
+WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
+See the License for the specific language governing permissions and
+limitations under the License.
+==============================================================================*/
+
+#include <memory>
+#include <string>
+#include <utility>
+#include <vector>
+#include <map>
+
+#include "tensorflow/compiler/aot/flags.h"
+#include "tensorflow/compiler/tf2xla/tf2xla.h"
+#include "tensorflow/compiler/tf2xla/tf2xla_util.h"
+#include "tensorflow/core/platform/init_main.h"
+
+namespace tensorflow {
+namespace xlacompile {
+
+const char kUsageHeader[] =
+    "xlacompile performs ahead-of-time compilation of a TensorFlow graph,\n"
+    "resulting in an object file compiled for your target architecture, and a\n"
+    "header file that gives access to the functionality in the object file.\n"
+    "A typical invocation looks like this:\n"
+    "\n"
+    "   $ xlacompile --graph=mygraph.pb --config=config.pbtxt --output=output.pbtxt\n"
+    "\n";
+
+void AppendMainFlags(std::vector<Flag>* flag_list, tfcompile::MainFlags* flags) {
+  const std::vector<Flag> tmp = {
+      {"graph", &flags->graph,
+       "Input GraphDef file. If the file ends in '.pbtxt' it is expected to "
+       "be in the human-readable proto text format, otherwise it is expected "
+       "to be in the proto binary format."},
+      {"config", &flags->config,
+       "Input file containing Config proto. If the file ends in '.pbtxt' it "
+       "is expected to be in the human-readable proto text format, otherwise "
+       "it is expected to be in the proto binary format."},
+      {"output", &flags->out_session_module,
+       "Output session module proto. Will generate '.pb' and '.pbtxt' file."},
+  };
+  flag_list->insert(flag_list->end(), tmp.begin(), tmp.end());
+}
+
+Status ReadProtoFile(const string& fname, protobuf::Message* proto) {
+  if (absl::EndsWith(fname, ".pbtxt")) {
+    return ReadTextProto(Env::Default(), fname, proto);
+  } else {
+    return ReadBinaryProto(Env::Default(), fname, proto);
+  }
+}
+
+Status Main(tfcompile::MainFlags& flags) {
+  // Process config.
+  tf2xla::Config config;
+  if (flags.config.empty()) {
+    return errors::InvalidArgument("Must specify --config");
+  }
+  TF_RETURN_IF_ERROR(ReadProtoFile(flags.config, &config));
+  TF_RETURN_IF_ERROR(ValidateConfig(config));
+  if (flags.dump_fetch_nodes) {
+    std::set<string> nodes;
+    for (const tf2xla::Fetch& fetch : config.fetch()) {
+      nodes.insert(fetch.id().node_name());
+    }
+    std::cout << absl::StrJoin(nodes, ",");
+    return Status::OK();
+  }
+
+  // Read and initialize the graph.
+  if (flags.graph.empty()) {
+    return errors::InvalidArgument("Must specify --graph");
+  }
+  if (flags.out_session_module.empty()) {
+    return errors::InvalidArgument("Must specify --output");
+  }
+
+  string output_pb_bin = flags.out_session_module + ".pb";
+  string output_pb_txt = flags.out_session_module + ".pbtxt";
+  if (output_pb_bin == flags.config || output_pb_bin == flags.graph ||
+      output_pb_txt == flags.config || output_pb_txt == flags.graph) {
+    return errors::InvalidArgument("Must different --config --graph --output");
+  }
+
+  GraphDef graph_def;
+  TF_RETURN_IF_ERROR(ReadProtoFile(flags.graph, &graph_def));
+  std::unique_ptr<Graph> graph;
+  TF_RETURN_IF_ERROR(ConvertGraphDefToXla(graph_def, config, graph));
+
+  std::map<string, string> arg_name_maps;
+  GraphDef new_graph_def;
+  graph->ToGraphDef(&new_graph_def);
+  // Delete _class attribute for: expects to be colocated with unknown node
+  for (int i = 0; i < new_graph_def.node_size(); ++i) {
+    NodeDef *node = new_graph_def.mutable_node(i);
+    node->mutable_attr()->erase("_class");
+    if (node->op() == "_Retval") {
+      node->set_name(absl::StrCat(node->attr().at("index").i(), "_Retval"));
+    }
+    if (node->op() == "_Arg") {
+      const string name = node->name();
+      node->set_name(absl::StrCat(node->attr().at("index").i(), "_Arg"));
+      arg_name_maps[name] = node->name();
+    }
+  }
+
+  for (int i = 0; i < new_graph_def.node_size() && !arg_name_maps.empty(); ++i) {
+    NodeDef *node = new_graph_def.mutable_node(i);
+    for (int j = 0; j < node->input_size(); ++j) {
+      auto it = arg_name_maps.find(node->input(j));
+      if (it != arg_name_maps.end()) {
+        *node->mutable_input(j) = it->second;
+      }
+    }
+  }
+
+  TF_RETURN_IF_ERROR(WriteBinaryProto(Env::Default(), output_pb_bin, new_graph_def));
+  std::cerr << "Successfully convert: " << output_pb_bin << "\n";
+  TF_RETURN_IF_ERROR(WriteTextProto(Env::Default(), output_pb_txt, new_graph_def));
+  std::cerr << "Successfully convert: " << output_pb_txt << "\n";
+  return Status::OK();
+}
+
+}  // end namespace xlacompile
+}  // end namespace tensorflow
+
+int main(int argc, char** argv) {
+  tensorflow::tfcompile::MainFlags flags;
+  flags.target_triple = "x86_64-pc-linux";
+  flags.out_function_object = "out_model.o";
+  flags.out_metadata_object = "out_helper.o";
+  flags.out_header = "out.h";
+  flags.entry_point = "entry";
+
+  std::vector<tensorflow::Flag> flag_list;
+  tensorflow::xlacompile::AppendMainFlags(&flag_list, &flags);
+
+  tensorflow::string usage = tensorflow::xlacompile::kUsageHeader;
+  usage += tensorflow::Flags::Usage(argv[0], flag_list);
+  if (argc > 1 && absl::string_view(argv[1]) == "--help") {
+    std::cerr << usage << "\n";
+    return 0;
+  }
+  bool parsed_flags_ok = tensorflow::Flags::Parse(&argc, argv, flag_list);
+  QCHECK(parsed_flags_ok) << "\n" << usage;
+
+  tensorflow::port::InitMain(usage.c_str(), &argc, &argv);
+  QCHECK(argc == 1) << "\nERROR: This command does not take any arguments "
+                       "other than flags\n\n"
+                    << usage;
+  tensorflow::Status status = tensorflow::xlacompile::Main(flags);
+  if (status.code() == tensorflow::error::INVALID_ARGUMENT) {
+    std::cerr << "INVALID ARGUMENTS: " << status.error_message() << "\n\n"
+              << usage;
+    return 1;
+  } else {
+    TF_QCHECK_OK(status);
+  }
+  return 0;
+}

diff --git a/tensorflow/compiler/tf2xla/tf2xla.cc b/tensorflow/compiler/tf2xla/tf2xla.cc
index 3c2b256..3872776 100644
--- a/tensorflow/compiler/tf2xla/tf2xla.cc
+++ b/tensorflow/compiler/tf2xla/tf2xla.cc
@@ -410,4 +410,10 @@ Status ConvertGraphDefToXla(const GraphDef& graph_def,
   return Status::OK();
 }

+Status ConvertGraphDefToXla(const GraphDef &graph_def,
+                            const tf2xla::Config &config,
+                            std::unique_ptr<Graph> &graph) {
+  return InitGraph(graph_def, config, &graph);
+}
+
 }  // namespace tensorflow

diff --git a/tensorflow/compiler/tf2xla/tf2xla.h b/tensorflow/compiler/tf2xla/tf2xla.h
index 432a12a..969500c 100644
--- a/tensorflow/compiler/tf2xla/tf2xla.h
+++ b/tensorflow/compiler/tf2xla/tf2xla.h
@@ -20,6 +20,7 @@ limitations under the License.
 #include "tensorflow/compiler/xla/client/client.h"
 #include "tensorflow/compiler/xla/client/xla_computation.h"
 #include "tensorflow/core/framework/graph.pb.h"
+#include "tensorflow/core/graph/graph.h"

 namespace tensorflow {

@@ -34,6 +35,9 @@ Status ConvertGraphDefToXla(const GraphDef& graph_def,
                             const tf2xla::Config& config, xla::Client* client,
                             xla::XlaComputation* computation);

+Status ConvertGraphDefToXla(const GraphDef &graph_def,
+                            const tf2xla::Config &config,
+                            std::unique_ptr<Graph> &graph);
 }  // namespace tensorflow

 #endif  // TENSORFLOW_COMPILER_TF2XLA_TF2XLA_H_

--
1.8.3.1

```
