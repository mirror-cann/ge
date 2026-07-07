# 后续版本废弃配置

## INPUT\_SHAPE\_RANGE

**该参数已废弃，请勿使用。**
若涉及指定模型输入数据的shape范围，请使用INPUT\_SHAPE参数。

指定模型输入数据的shape range。该功能不能与DYNAMIC\_BATCH\_SIZE、DYNAMIC\_IMAGE\_SIZE、DYNAMIC\_DIMS同时使用。

- 支持按照name设置："input\_name1:\[n1,c1,h1,w1\];input\_name2:\[n2,c2,h2,w2\]"，例如："input\_name1:\[8\~20,3,5,-1\];input\_name2:\[5,3\~9,10,-1\]"。指定的节点必须放在双引号中，节点中间使用英文分号分隔。input\_name必须是转换前的网络模型中的节点名称，shape range值必须放在\[\]中。如果用户知道data节点的name，推荐按照name设置INPUT\_SHAPE\_RANGE。
- 支持按照index设置："\[n1,c1,h1,w1\],\[n2,c2,h2,w2\]"，例如："\[8\~20,3,5,-1\],\[5,3\~9,10,-1\]"。可以不指定节点名，默认第一个中括号为第一个输入节点，节点中间使用英文逗号分隔。按照index设置INPUT\_SHAPE\_RANGE时，data节点需要设置属性index，说明是第几个输入，index从0开始。
- 动态维度有shape范围的用波浪号“\~”表示，固定维度用固定数字表示，无限定范围的用-1表示；
- 对于标量输入，也需要填入shape范围，表示方法为：\[\]；
- 对于多输入场景，例如有三个输入时，如果只有第二个第三个输入具有shape范围，第一个输入为固定输入时，仍需要将固定输入shape填入。

## SHAPE\_GENERALIZED\_BUILD\_MODE

图编译时Shape的编译方式。**该参数在后续版本废弃、新开发功能请不要使用该参数。**

- shape\_generalized：模糊编译，是指对于支持动态Shape的算子，在编译时系统内部对可变维度做了泛化后再进行编译。

    该参数使用场景为：用户想编译一次达到多次执行推理的目的时，可以使用模糊编译特性。

- shape\_precise：精确编译，是指按照用户指定的维度信息、在编译时系统内部不做任何转义直接编译。
