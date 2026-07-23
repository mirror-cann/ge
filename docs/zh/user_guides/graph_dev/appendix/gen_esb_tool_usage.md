# gen\_esb二进制工具使用

gen\_esb是ES API的代码生成二进制工具，根据ASCEND\_OPP\_PATH环境变量读取CANN软件包内置的op\_graph子目录下的算子原型so，根据so里面存在的原型信息生成对应的ES产物； 如果是自定义的op\_proto的原型so，推荐使用generate\_es\_package.cmake提供的[add\_es\_library](../es_graph/generate_es_graph_api_optional.md)来生成ES的产物并编译。生成产物包括：

- 所有支持的算子（ops）的C接口。
- 所有支持的算子的C++接口。
- 所有支持的算子的Python接口。
- 聚合头文件，方便用户一次性包含所有算子。
- 聚合Python文件，方便用户一次性导入所有算子。

ASCEND\_OPP\_PATH环境变量详细说明请参见[《环境变量参考》](https://hiascend.com/document/redirect/CannCommunityEnvRef)。

## 使用前提

- 在使用该工具之前，请先安装CANN软件包，安装完成后，该工具安装在`$\{INSTALL\_DIR\}/bin`目录。其中，`$\{INSTALL\_DIR\}`请替换为CANN软件安装后文件存储路径。以root用户安装为例，安装后文件默认存储路径为：/usr/local/Ascend/cann。
- 设置环境变量（如下环境变量以root用户为例，且CANN软件包安装在默认路径）

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

执行完上述步骤后，在CANN软件包所在服务器任意路径执行**gen\_esb --help**命令，若提示类似如下信息，则说明工具能正常使用：

```text
ES Graph Builder Code Generator v1.0
Usage: ./gen_esb [--output_dir=DIR] [--module_name=NAME] [--h_guard_prefix=PREFIX] [--exclude_ops=OP_TYPE]
... ...
```

## 使用样例及参数说明

工具使用命令为：

```bash
./gen_esb [--output_dir=DIR] [--module_name=NAME] [--h_guard_prefix=PREFIX] [--exclude_ops=OP_TYPE1,OP_TYPE2]
```

参数说明如下：

- --output\_dir：可选参数，指定代码生成的目标目录，如果不指定，默认输出到当前目录。
- --module\_name：可选参数，控制聚合头文件的命名：
  - "math" -\> es\_math\_ops\_c.h、es\_math\_ops.h、es\_math\_ops.py
  - "all" -\> es\_all\_ops\_c.h、es\_all\_ops.h、es\_all\_ops.py
  - 不传递 -\> 默认为"all"

- --h\_guard\_prefix：可选参数，控制生成的头文件保护宏前缀，用于可能的内外部算子同名情况的区分：
  - 如果不指定，使用默认前缀
  - 指定时，拼接默认前缀
  - python文件不感知此参数，同名场景通过不同的路径避免冲突

- --exclude\_ops：可选参数，控制排除生成的算子。根据','分隔算子名。

使用示例如下：

- 生成到当前目录，使用默认模块名"all"，默认保护宏前缀

    ```bash
    ./gen_esb
    ```

- 生成到指定目录，使用默认模块名"all"，默认保护宏前缀

    ```bash
    ./gen_esb --output_dir=./output
    ```

- 生成到指定目录，使用"math"模块名，默认保护宏前缀

    ```bash
    ./gen_esb --output_dir=./output --module_name=math
    ```

- 生成到指定目录，使用"all"模块名，默认保护宏前缀

    ```bash
    ./gen_esb --output_dir=./output --module_name=all
    ```

- 生成到指定目录，使用"math"模块名，自定义保护宏前缀"MY\_CUSTOM"

    ```bash
    ./gen_esb --output_dir=./output --module_name=math --h_guard_prefix=MY_CUSTOM
    ```

- 生成到指定目录，使用"math"模块名，自定义保护宏前缀"MY\_CUSTOM"，并排除Add算子生成

    ```bash
    ./gen_esb --output_dir=./output --module_name=math --h_guard_prefix=MY_CUSTOM --exclude_ops=Add
    ```

## 输出文件

- es\_\{module\_name\}\_ops\_c.h：C接口聚合头文件
- es\_\{module\_name\}\_ops.h：C++接口聚合头文件
- es\_\{module\_name\}\_ops.py：Python接口聚合文件
- es\_<op\_type\>\_c.h：单个算子的C接口头文件
- es\_<op\_type\>.cpp：单个算子的C接口实现文件
- es\_<op\_type\>.h：单个算子的C++接口头文件
- es\_<op\_type\>.py：单个算子的Python接口文件

> [!NOTE]说明
>推荐使用CMake函数封装的方式，因为CMake函数内部保持了Python依赖的so的名字和ES产物编译出来的so的名字一致； 否则需手动在CMake工程中将ES生成的so名字命名为libes\_\{module\_name\}.so，以保证一致性。
