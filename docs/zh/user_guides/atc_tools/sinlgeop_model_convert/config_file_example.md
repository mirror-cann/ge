# 配置文件样例

## 单算子描述文件配置

不同输入或者不同Format场景，单算子描述文件配置不同，本章节给出各场景的配置示例。

本章节中的单算子是基于Ascend IR定义的，描述文件为JSON格式。关于JSON描述文件中各参数的解释请参见[表1](singleop_desc_intro.md#table1)，关于单算子的Ascend IR定义请参见[《算子库》](https://hiascend.com/document/redirect/CannCommunityOplist)  \>“Ascend IR算子规格说明”  。

- **Format为ND：**

    该示例中的单算子转换后的离线模型为：add.om

    ```json
    [
        {
          "op": "Add",
          "name": "add",
          "input_desc": [
            {
              "format": "ND",
              "shape": [3,3],
              "type": "int32"
            },
            {
              "format": "ND",
              "shape": [3,3],
              "type": "int32"
            }
          ],
          "output_desc": [
            {
              "format": "ND",
              "shape": [3,3],
              "type": "int32"
            }
          ]
        }
    ]
    ```

- **Format为NCHW：**

    该示例中的单算子转换后的离线模型为：conv2d.om

    ```json
    [
     {
       "op": "Conv2D",
       "name": "conv2d",
       "input_desc": [
         {
           "format": "NCHW",
           "shape": [1, 3, 16, 16],
           "type": "float16"
         },
         {
           "format": "NCHW",
           "shape": [3, 3, 3, 3],
           "type": "float16"
         }
       ],
       "output_desc": [
         {
           "format": "NCHW",
           "shape": [1, 3, 16, 16],
           "type": "float16"
         }
       ],
       "attr": [
           {
             "name": "strides",
             "type": "list_int",
             "value": [1, 1, 1, 1]
           },
           {
             "name": "pads",
             "type": "list_int",
             "value": [1, 1, 1, 1]
           },
           {
             "name": "dilations",
             "type": "list_int",
             "value": [1, 1, 1, 1]
           }
       ]
     }
     ]
    ```

- **Tensor计算过程中使用的Format与原始Format不同**

    ATC模型转换时，会将**origin\_format**与**origin\_shape**转成离线模型需要的**format**与**shape**。

    该示例中的单算子转换后的离线模型为：add.om

    ```json
    [
      {
        "op": "Add",
        "name": "add",
        "input_desc": [
          {
            "format": "NC1HWC0",
            "origin_format": "NCHW",
            "shape": [8, 1, 16, 4, 16],
            "origin_shape": [8, 16, 16, 4],
            "type": "float16"
          },
          {
            "format": "NC1HWC0",
            "origin_format": "NCHW",
            "shape": [8, 1, 16, 4, 16],
            "origin_shape": [8, 16, 16, 4],
            "type": "float16"
          }
        ],
        "output_desc": [
          {
            "format": "NC1HWC0",
            "origin_format": "NCHW",
            "shape": [8, 1, 16, 4, 16],
            "origin_shape": [8, 16, 16, 4],
            "type": "float16"
          }
        ]
      }
    ]
    ```

- **输入指定为常量**

    该场景下，支持设置为常量的输入，新增**is\_const**和**const\_value**两个参数，分别表示是否为常量以及常量取值，**const\_value**当前仅支持一维list配置，具体配置个数由shape取值决定，例如，如下样例中shape为2，则**const\_value**中列表个数为2；**const\_value**中取值类型由type决定，假设type取值为float16，则单算子编译时会自动将**const\_value**中的取值转换为float16格式的取值。

    该示例中的单算子转换后的离线模型为：resizeBilinearV2.om

    ```json
    [
      {
        "op": "ResizeBilinearV2",
        "name": "resizeBilinearV2",
        "input_desc": [
          {
            "format": "NHWC",
            "name": "x",
            "shape": [
              4,
              16,
              16,
              16
            ],
            "type": "float16"
          },
          {
            "format": "NHWC",
            "is_const": true,
            "const_value": [49, 49],
            "name": "size",
            "shape": [
              2
            ],
            "type": "int32"
          }
        ],
        "output_desc": [
          {
            "format": "NHWC",
            "name": "y",
            "shape": [
              4,
              48,
              48,
              16
            ],
            "type": "float"
          }
        ],
        "attr": [
          {
            "name": "align_corners",
            "type": "bool",
            "value": false
          },
          {
            "name": "half_pixel_centers",
            "type": "bool",
            "value": false
          }
        ]
      }
    ]
    ```

- **可选输入（optional input）：**

    当存在可选输入，且可选输入没有输入数据时，则必须将可选输入的format配置为RESERVED，同时将type配置为UNDEFINED；若可选输入有输入数据时，则按其输入数据的format、type配置即可。

    该示例中的单算子转换后的离线模型为：matMulV2.om

    ```json
    [
      {
        "op": "MatMulV2",
        "name": "matMulV2",
        "input_desc": [
          {
            "format": "ND",
            "shape": [16, 16],
            "type": "float"
          },
          {
            "format": "ND",
            "shape": [16, 16],
            "type": "float"
          },
          {
            "format": "RESERVED",
            "shape": [],
            "type": "UNDEFINED"
          },
          {
            "format": "RESERVED",
            "shape": [],
            "type": "UNDEFINED"
          }
        ],
        "attr": [
        {
            "name": "transpose_x1",
            "type": "bool",
            "value": false
        },
        {
            "name": "transpose_x2",
            "type": "bool",
            "value": false
        }
        ],
        "output_desc": [
          {
            "format": "ND",
            "shape": [16, 16],
            "type": "float"
          }
     ]
      }
    ]

    ```

- **输入个数不确定（动态输入场景）**：

    该场景下，单算子的输入个数不确定。此处以AddN单算子为例。该示例中的单算子转换后的离线模型为：addN.om

  - 构造的单算子JSON文件使用动态输入dynamic\_input参数，而不使用Tensor的名称name参数。

    该场景下算子的dynamic\_input取值必须和算子信息库中该算子定义的输入name的取值相同。具体设置几个输入，由AddN单算子描述文件属性参数中**N**的取值决定，用户可以自行修改输入的个数，但是必须和属性中N的取值匹配。（该说明仅针对AddN算子生效，其他动态输入算子的约束以具体算子为准。）

      ```json
      [
          {
              "op": "AddN",
              "name": "addN",
              "input_desc": [
                  {
                      "dynamic_input": "x",
                      "format": "NCHW",
                      "shape": [1,3,166,166],
                      "type": "float32"
                  },
                  {
                      "dynamic_input": "x",
                      "format": "NCHW",
                      "shape": [1,3,166,166],
                      "type": "int32"
                  },
                  {
                      "dynamic_input": "x",
                      "format": "NCHW",
                      "shape": [1,3,166,166],
                      "type": "float32"
                  }
              ],
              "output_desc": [
                  {
                      "format": "NCHW",
                      "shape": [1,3,166,166],
                      "type": "float32"
                  }
              ],
              "attr": [
                  {
                      "name": "N",
                      "type": "int",
                      "value": 3
                  }
              ]
          }
      ]
      ```

  - 构造的单算子JSON文件使用Tensor的名称name参数，而不使用动态输入dynamic\_input参数。

    该场景下算子的name取值必须和算子原型定义中算子的输入名称相同，根据输入的个数自动生成x0、x1、x2……。具体设置几个Tensor名称，由AddN单算子描述文件属性参数中**N**的取值决定，用户可以自行修改Tensor名称的个数，但是必须和属性中N的取值匹配，例如N取值为3，则name取值分别设置为x0、x1、x2。（该说明仅针对AddN算子生效，其他动态输入算子的约束以具体算子为准。）

      ```json
        [
            {
                "op": "AddN",
                "name": "addN",
                "input_desc": [
                    {
                        "name":"x0",
                        "format": "NCHW",
                        "shape": [1,3,166,166],
                        "type": "float32"
                    },
                    {
                        "name":"x1",
                        "format": "NCHW",
                        "shape": [1,3,166,166],
                        "type": "int32"
                    },
                    {
                        "name":"x2",
                        "format": "NCHW",
                        "shape": [1,3,166,166],
                        "type": "float32",

                    }
                ],
                "output_desc": [
                    {
                        "format": "NCHW",
                        "shape": [1,3,166,166],
                        "type": "float32"
                    }
                ],
                "attr": [
                    {
                        "name": "N",
                        "type": "int",
                        "value": 3
                    }
                ]
            }
        ]
        ```

## 多组算子描述文件配置

描述文件支持定义多组算子JSON文件配置，一组配置包括算子类型、算子输入和输出信息、视算子情况决定是否包括属性信息。

如果JSON文件配置了多组算子，则模型转换完成后，会生成多组算子对应的om离线模型文件。如下配置文件只是样例，请根据实际情况进行修改。

```json
[
  {
    "op": "MatMul",
    "name": "matMul01",
    "input_desc": [
      {
        "format": "ND",
        "shape": [
          16,
          16
        ],
        "type": "float16"
      },
    ...
    ],
    "output_desc": [
      {
        "format": "ND",
        "shape": [
          16,
          16
        ],
        "type": "float16"
      }
    ],
    "attr": [
      {
        "name": "alpha",
        "type": "float",
        "value": 1.0
      },
    ...
    ]
  },
  {
    "op": "MatMul",
    "name": "matMul02",
    "input_desc": [
      {
        "format": "ND",
        "shape": [
          256,
          256
        ],
        "type": "float16"
      },
    ...
    ],
    "output_desc": [
      {
        "format": "ND",
        "shape": [
          256,
          256
        ],
        "type": "float16"
      }
    ],
    "attr": [
      {
        "name": "alpha",
        "type": "float",
        "value": 1.0
      },
    ...
    ]
  }
]
```

## 动态Shape单算子描述文件配置

动态Shape场景，单算子描述文件根据场景不同，内容也有差异，本章节就给出不同场景下的配置样例。

- 模型编译时不指定Shape，模型执行时根据输入静态Shape，能推导出具体输出Shape：

    ```json
    [
      {
        "op": "Add",
        "name": "add",
        "input_desc": [
          {
            "format": "ND",
            "shape": [-1,16],
            "shape_range": [[0, 32]],
            "type": "int64"
          },
          {
            "format": "ND",
            "shape": [-1,16],
            "shape_range": [[0, 32]],
            "type": "int64"
          }
        ],
        "output_desc": [
          {
            "format": "ND",
            "shape": [-1,16],
            "shape_range": [[0,32]],
            "type": "int64"
          }
        ]
      }
    ]
    ```

- 模型编译时不指定Shape，模型执行时根据输入静态Shape和常量，能推导出具体输出Shape：

    ```json
    [
      {
        "op": "TopK",
        "name": "topK",
        "input_desc": [
          {
            "format": "ND",
            "shape": [-1],
           "shape_range": [[1,-1]],
            "type": "int32"
          },
          {
            "format": "ND",
            "shape": [],      # 推理时会传入常量
            "type": "int32"
          }
        ],
        "output_desc": [
          {
            "format": "ND",
            "shape": [-1],
            "shape_range": [[1,-1]],
            "type": "int32"
          },
       {
            "format": "ND",
            "shape": [-1],
            "shape_range": [[1,-1]],
            "type": "int32"
          }],
          "attr": [
          {
            "name": "sorted",
            "type": "bool",
            "value": true
          }
        ]
      }
    ]
    ```

- 模型编译时不指定Shape，模型执行时根据输入静态Shape，无法得到算子的准确输出Shape，但可以得到输出Shape的范围。

    该场景下在输出参数output\_desc中将算子输出TensorDesc中Shape为动态维度的维度值记为“-1”，并对其“-1”的维度给出shape\_range取值范围：

    ```json
    [
      {
        "op": "Where",
        "name": "where",
        "input_desc": [
          {
            "format": "ND",
            "shape": [-1],
            "shape_range": [[1,-1]],
            "type": "int32"
          }
        ],
        "output_desc": [
          {
            "format": "ND",
            "shape": [-1, 1],
            "shape_range": [[1,-1]],
            "type": "int64"
          }
        ]
      }
    ]
    ```
