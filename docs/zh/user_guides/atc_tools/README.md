# ATC离线模型编译工具用户指南

- [学习向导](overview/learning_guide.md)
- [ATC概述](overview/atc_overview.md)
- [准备环境](overview/environment_setup.md)
- [快速入门](overview/quick_start.md)

- [初级功能](basic_feature/basic_features.md)
- [高级功能](AIPP/advanced_features.md)
  - [开启AIPP](AIPP/enable_aipp.md)
    - [什么是AIPP](AIPP/aipp_intro.md)
    - [AIPP配置示例](AIPP/aipp_config_example.md)
    - [如何开启AIPP](AIPP/how_to_enable_aipp.md)
    - [色域转换配置说明](AIPP/CSC_config.md)
    - [归一化配置说明](AIPP/normalization_config.md)
    - [Crop/Padding配置说明](AIPP/crop_padding_config.md)
    - [配置文件模板](AIPP/config_file_template.md)
    - [典型场景样例参考](AIPP/typical_scenario_examples.md)

  - [单算子模型转换](sinlgeop_model_convert/sinlgeop_model_convert.md)
    - [什么是单算子描述文件](sinlgeop_model_convert/singleop_desc_intro.md)
    - [配置文件样例](sinlgeop_model_convert/config_file_example.md)
    - [如何将算子描述文件转成离线模型](sinlgeop_model_convert/convert_singleop_desc_to_om.md)

- [参数说明<a name="sub_menu"></a>](CLI_options/README.md)

- [专题](custom_network_modify_caffe/special_topics.md)
  - [定制网络修改（Caffe）<a name="sub_menu"></a>](custom_network_modify_caffe/README.md)
  - [定制网络修改（TensorFlow）<a name="sub_menu"></a>](custom_network_modify_tf/README.md)

- [参考](references/references_index.md)
  - [dump图详细信息](references/dump_graph_details.md)
  - [融合规则列表](references/fusion_rules_list.md)
  - [支持量化的层及约束](references/quantization_layers_constraints.md)
  - [量化简易配置文件](references/quantization_simple_config.md)
  - [开启AI CPU Cast算子自动插入特性](references/enable_ai_cpu_cast_auto_insert.md)

- [FAQ<a name="sub_menu"></a>](FAQ/README.md)
