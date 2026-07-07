# 头文件和库文件说明

## 接口分类

接口名以acl作为前缀，命名风格为：acl+_接口类别缩写_+\*，其中，\*表示操作动词和对象，均采用首字母大写。下文为了描述方便，将本文中的接口统称为acl接口。

**表 1**  关键接口类别

| 接口名前缀 | 描述 |
| --- | --- |
| aclmdl | 模型推理类的接口 |
| aclop | 单算子模型执行类的接口 |
| aclblas | blas类接口 |

## 调用接口依赖的头文件和库文件说明

安装固件、驱动及CANN软件包后，编译、运行应用程序时才能引用到acl接口的头文件、库文件。

您需要根据实际使用的acl接口来include依赖的文件，各头文件的用途如下表所示。

acl接口的头文件在“$\{INSTALL\_DIR\}/include/”目录下，库文件在“$\{INSTALL\_DIR\}/lib64/”目录下。$\{INSTALL\_DIR\}请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。

**表 2**  头文件列表

| 定义接口的头文件 | 用途 | 对应的库文件 |
| --- | --- | --- |
| acl/acl_mdl.h | 用于定义模型管理接口。 | libacl_mdl.so<br>说明：为了兼容旧版本，旧版本中支持使用libascendcl.so，但后续版本这种方式会废弃，建议使用libacl_mdl.so，防止后续版本出现兼容性问题。 |
| acl/acl_op.h<br>acl/acl_op_compiler.h | 用于定义单算子调用接口（仅包含单算模型执行接口）。 | libacl_op_executor.so<br>libacl_op_compiler.so<br>说明：为了兼容旧版本，旧版本中支持使用libascendcl.so，但后续版本这种方式会废弃，建议使用libacl_op_executor.so和libacl_op_compiler.so，防止后续版本出现兼容性问题。 |
| acl/ops/acl_cblas.h | 用于定义CBLAS接口。 | libacl_cblas.so |

**注意：**
编译acl接口程序时，请按照include的头文件依赖对应的库文件，如果引用多余的so文件（例如libascendcl.a），可能导致版本功能异常或后续版本升级时存在兼容性问题。
