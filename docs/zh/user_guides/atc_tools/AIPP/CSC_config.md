# 色域转换配置说明

AIPP提供了更为方便的图像格式转换方式：色域转换，用于将输入的图片格式，转换为模型需要的图片格式，一旦确认了AIPP处理前与AIPP处理后的图片格式，即可确定色域转换相关的参数值（**matrix\_r\*c\*配置项的值是固定的，不需要调整**）。

例如：将JPEG格式的图像文件（如后缀为jpg、jpeg、JPG、JPEG的图像文件）转为RGB格式；将视频解码后的YUV格式数据转为RGB格式。而根据不同的彩色视频数字化标准又可以将视频格式分为BT-601标准清晰度视频格式（定义于SDTV标准中）和BT-709高清晰度视频格式（定义于HDTV标准中）。两种视频格式又分为NARROW和WIDE，其中：

NARROW取值范围为：![图示](../figures/csc_config_narrow.png)，
WIDE取值范围为：![图示](../figures/csc_config_wide.png)

关于如何判断输入数据的标准，请参见[使用AIPP色域转换模型时如何判断视频流的格式标准](../FAQ/aipp_video_format_standard.md)。

YUV格式的数据转为RGB格式可以视作如下公式展示的矩阵乘法，这其中的转换矩阵就是待配置的参数和偏移量。

```text
# YUV转BGR：
| B |   | matrix_r0c0 matrix_r0c1 matrix_r0c2 | | Y - input_bias_0 |
| G | = | matrix_r1c0 matrix_r1c1 matrix_r1c2 | | U - input_bias_1 | >> 8
| R |   | matrix_r2c0 matrix_r2c1 matrix_r2c2 | | V - input_bias_2 |
```

在AIPP处理前，针对模型输入的图片或视频（各颜色编码方式，如YUV420SP\_U8、RGB888\_U8等），当前给出JPEG、BT-601NARROW、BT-601WIDE、BT-709NARROW、BT-709WIDE几种典型场景下的色域转换配置。

> [!NOTE]说明
>
>DVPP处理后的数据，不会对色域配置产生影响，例如DVPP处理前的数据为BT-709格式，经过DVPP处理后，输入给AIPP，此时数据仍为BT-709格式。

## 色域转换概览

支持的色域转换配置列表如下所示：

- [YUV420SP\_U8转YUV444](#yuv420sp_u8转yuv444)
- [YUV420SP\_U8转YVU444](#yuv420sp_u8转yvu444)
- [YUV420SP\_U8转RGB](#yuv420sp_u8转rgb)
- [YUV420SP\_U8转BGR](#yuv420sp_u8转bgr)
- [YUV420SP\_U8转GRAY](#yuv400_u8转gray)
- [YVU420SP\_U8转RGB](#yvu420sp_u8转rgb)
- [YVU420SP\_U8转BGR](#yvu420sp_u8转bgr)
- [RGB888\_U8转RGB](#rgb888_u8转rgb)
- [RGB888\_U8转BGR](#rgb888_u8转bgr)
- [RGB888\_U8转YUV444](#rgb888_u8转yuv444)
- [RGB888\_U8转YVU444](#rgb888_u8转yvu444)
- [RGB888\_U8转GRAY](#rgb888_u8转gray)
- [BGR888\_U8转GRAY](#bgr888_u8转gray)
- [BGR888\_U8转RGB](#bgr888_u8转rgb)
- [BGR888\_U8转BGR](#bgr888_u8转bgr)
- [XRGB8888\_U8转RGB](#xrgb8888_u8转rgb)
- [XRGB8888\_U8转BGR](#xrgb8888_u8转bgr)
- [XRGB8888\_U8转YUV444](#rgb888_u8转yuv444)
- [XRGB8888\_U8转YVU444](#rgb888_u8转yvu444)
- [XRGB8888\_U8转GRAY](#xrgb8888_u8转gray)
- [XBGR8888\_U8转GRAY](#xbgr8888_u8转gray)
- [RGBX8888\_U8转GRAY](#rgbx8888_u8转gray)
- [BGRX8888\_U8转GRAY](#bgrx8888_u8转gray)
- [YUV400\_U8转GRAY](#yuv400_u8转gray)
- [RGB888\_U8转FP16 RGB](#rgb888_u8转fp16-rgb)

## YUV420SP\_U8转YUV444

```textproto
aipp_op {
    aipp_mode: static
    input_format : YUV420SP_U8
    csc_switch : false
    rbuv_swap_switch : false
}
```

## YUV420SP\_U8转YVU444

```textproto
aipp_op {
    aipp_mode: static
    input_format : YUV420SP_U8
    csc_switch : false
    rbuv_swap_switch : true
}
```

## YUV420SP\_U8转RGB

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 359<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 454<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 409<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -100<br>   matrix_r1c2 : -208<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 516<br>   matrix_r2c2 : 0<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 359<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 454<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 459<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -55<br>   matrix_r1c2 : -136<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 541<br>   matrix_r2c2 : 0<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 403<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -48<br>   matrix_r1c2 : -120<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 475<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

## YUV420SP\_U8转BGR

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 454<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 359<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 516<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -100<br>   matrix_r1c2 : -208<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 409<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 454<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 359<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 541<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -55<br>   matrix_r1c2 : -136<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 459<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 475<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -48<br>   matrix_r1c2 : -120<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 403<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

## YUV420SP\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : YUV420SP_U8
    csc_switch : true
    rbuv_swap_switch : false
    matrix_r0c0 : 256
    matrix_r0c1 : 0
    matrix_r0c2 : 0
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    input_bias_0 : 0
    input_bias_1 : 0
    input_bias_2 : 0
}
```

## YVU420SP\_U8转RGB

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 359<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 454<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 409<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -100<br>   matrix_r1c2 : -208<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 516<br>   matrix_r2c2 : 0<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 359<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 454<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 459<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -55<br>   matrix_r1c2 : -136<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 541<br>   matrix_r2c2 : 0<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 0<br>   matrix_r0c2 : 403<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -48<br>   matrix_r1c2 : -120<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 475<br>   matrix_r2c2 : 0<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

## YVU420SP\_U8转BGR

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 454<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 359<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 516<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -100<br>   matrix_r1c2 : -208<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 409<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 454<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -88<br>   matrix_r1c2 : -183<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 359<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 298<br>   matrix_r0c1 : 541<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 298<br>   matrix_r1c1 : -55<br>   matrix_r1c2 : -136<br>   matrix_r2c0 : 298<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 459<br>   input_bias_0 : 16<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : YUV420SP_U8<br>   csc_switch : true<br>   rbuv_swap_switch : true<br>   matrix_r0c0 : 256<br>   matrix_r0c1 : 475<br>   matrix_r0c2 : 0<br>   matrix_r1c0 : 256<br>   matrix_r1c1 : -48<br>   matrix_r1c2 : -120<br>   matrix_r2c0 : 256<br>   matrix_r2c1 : 0<br>   matrix_r2c2 : 403<br>   input_bias_0 : 0<br>   input_bias_1 : 128<br>   input_bias_2 : 128<br>   } |

## RGB888\_U8转RGB

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : false
    rbuv_swap_switch : false
}
```

## RGB888\_U8转BGR

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : false
    rbuv_swap_switch : true
}
```

## RGB888\_U8转YUV444

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : -43<br>   matrix_r1c1 : -85<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -107<br>   matrix_r2c2 : -21<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 66<br>   matrix_r0c1 : 129<br>   matrix_r0c2 : 25<br>   matrix_r1c0 : -38<br>   matrix_r1c1 : -74<br>   matrix_r1c2 : 112<br>   matrix_r2c0 : 112<br>   matrix_r2c1 : -94<br>   matrix_r2c2 : -18<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : -43<br>   matrix_r1c1 : -85<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -107<br>   matrix_r2c2 : -21<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 47<br>   matrix_r0c1 : 157<br>   matrix_r0c2 : 16<br>   matrix_r1c0 : -26<br>   matrix_r1c1 : -87<br>   matrix_r1c2 : 112<br>   matrix_r2c0 : 112<br>   matrix_r2c1 : -102<br>   matrix_r2c2 : -10<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 55<br>   matrix_r0c1 : 183<br>   matrix_r0c2 : 19<br>   matrix_r1c0 : -29<br>   matrix_r1c1 : -99<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -116<br>   matrix_r2c2 : -12<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

## RGB888\_U8转YVU444

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -107<br>   matrix_r1c2 : -21<br>   matrix_r2c0 : -43<br>   matrix_r2c1 : -85<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 66<br>   matrix_r0c1 : 129<br>   matrix_r0c2 : 25<br>   matrix_r1c0 : 112<br>   matrix_r1c1 : -94<br>   matrix_r1c2 : -18<br>   matrix_r2c0 : -38<br>   matrix_r2c1 : -74<br>   matrix_r2c2 : 112<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -107<br>   matrix_r1c2 : -21<br>   matrix_r2c0 : -43<br>   matrix_r2c1 : -85<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 47<br>   matrix_r0c1 : 157<br>   matrix_r0c2 : 16<br>   matrix_r1c0 : 112<br>   matrix_r1c1 : -102<br>   matrix_r1c2 : -10<br>   matrix_r2c0 : -26<br>   matrix_r2c1 : -87<br>   matrix_r2c2 : 112<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : RGB888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   matrix_r0c0 : 55<br>   matrix_r0c1 : 183<br>   matrix_r0c2 : 19<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -116<br>   matrix_r1c2 : -12<br>   matrix_r2c0 : -29<br>   matrix_r2c1 : -99<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

## RGB888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : true
    rbuv_swap_switch : false
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## BGR888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : true
    rbuv_swap_switch : true
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## BGR888\_U8转RGB

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : false
    rbuv_swap_switch : true
}
```

## BGR888\_U8转BGR

```textproto
aipp_op {
    aipp_mode: static
    input_format : RGB888_U8
    csc_switch : false
    rbuv_swap_switch : false
}
```

## XRGB8888\_U8转RGB

```textproto
aipp_op {
    aipp_mode: static
    input_format : XRGB8888_U8
    csc_switch : false
    rbuv_swap_switch : false
    ax_swap_switch : true
}
```

## XRGB8888\_U8转BGR

```textproto
aipp_op {
aipp_mode: static
input_format : XRGB8888_U8
csc_switch : false
rbuv_swap_switch : true
ax_swap_switch : true
}
```

## XRGB8888\_U8转YUV444

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : -43<br>   matrix_r1c1 : -85<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -107<br>   matrix_r2c2 : -21<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 66<br>   matrix_r0c1 : 129<br>   matrix_r0c2 : 25<br>   matrix_r1c0 : -38<br>   matrix_r1c1 : -74<br>   matrix_r1c2 : 112<br>   matrix_r2c0 : 112<br>   matrix_r2c1 : -94<br>   matrix_r2c2 : -18<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : -43<br>   matrix_r1c1 : -85<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -107<br>   matrix_r2c2 : -21<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 47<br>   matrix_r0c1 : 157<br>   matrix_r0c2 : 16<br>   matrix_r1c0 : -26<br>   matrix_r1c1 : -87<br>   matrix_r1c2 : 112<br>   matrix_r2c0 : 112<br>   matrix_r2c1 : -102<br>   matrix_r2c2 : -10<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 55<br>   matrix_r0c1 : 183<br>   matrix_r0c2 : 19<br>   matrix_r1c0 : -29<br>   matrix_r1c1 : -99<br>   matrix_r1c2 : 128<br>   matrix_r2c0 : 128<br>   matrix_r2c1 : -116<br>   matrix_r2c2 : -12<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

## XRGB8888\_U8转YVU444

- 输入数据为JPEG图像

    | JPEG |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -107<br>   matrix_r1c2 : -21<br>   matrix_r2c0 : -43<br>   matrix_r2c1 : -85<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601NARROW视频

    | BT-601NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 66<br>   matrix_r0c1 : 129<br>   matrix_r0c2 : 25<br>   matrix_r1c0 : 112<br>   matrix_r1c1 : -94<br>   matrix_r1c2 : -18<br>   matrix_r2c0 : -38<br>   matrix_r2c1 : -74<br>   matrix_r2c2 : 112<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-601WIDE视频

    | BT-601WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 77<br>   matrix_r0c1 : 150<br>   matrix_r0c2 : 29<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -107<br>   matrix_r1c2 : -21<br>   matrix_r2c0 : -43<br>   matrix_r2c1 : -85<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709NARROW视频

    | BT-709NARROW |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 47<br>   matrix_r0c1 : 157<br>   matrix_r0c2 : 16<br>   matrix_r1c0 : 112<br>   matrix_r1c1 : -102<br>   matrix_r1c2 : -10<br>   matrix_r2c0 : -26<br>   matrix_r2c1 : -87<br>   matrix_r2c2 : 112<br>   output_bias_0 : 16<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

- 输入数据为BT-709WIDE视频

    | BT-709WIDE |
    | --- |
    | aipp_op {<br>   aipp_mode: static<br>   input_format : XRGB8888_U8<br>   csc_switch : true<br>   rbuv_swap_switch : false<br>   ax_swap_switch : true<br>   matrix_r0c0 : 55<br>   matrix_r0c1 : 183<br>   matrix_r0c2 : 19<br>   matrix_r1c0 : 128<br>   matrix_r1c1 : -116<br>   matrix_r1c2 : -12<br>   matrix_r2c0 : -29<br>   matrix_r2c1 : -99<br>   matrix_r2c2 : 128<br>   output_bias_0 : 0<br>   output_bias_1 : 128<br>   output_bias_2 : 128<br>   } |

## XRGB8888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : XRGB8888_U8
    csc_switch : true
    rbuv_swap_switch : false
    ax_swap_switch : true
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## XBGR8888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : XRGB8888_U8
    csc_switch : true
    rbuv_swap_switch : true
    ax_swap_switch : true
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## RGBX8888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : XRGB8888_U8
    csc_switch : true
    rbuv_swap_switch : false
    ax_swap_switch : false
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## BGRX8888\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : XRGB8888_U8
    csc_switch : true
    rbuv_swap_switch : true
    ax_swap_switch : false
    matrix_r0c0 : 76
    matrix_r0c1 : 150
    matrix_r0c2 : 30
    matrix_r1c0 : 0
    matrix_r1c1 : 0
    matrix_r1c2 : 0
    matrix_r2c0 : 0
    matrix_r2c1 : 0
    matrix_r2c2 : 0
    output_bias_0 : 0
    output_bias_1 : 0
    output_bias_2 : 0
}
```

## YUV400\_U8转GRAY

```textproto
aipp_op {
    aipp_mode: static
    input_format : YUV400_U8
    csc_switch : false
}
```

## RGB888\_U8转FP16 RGB

```textproto
aipp_op {
    aipp_mode: static
    related_input_rank: 0
    input_format : RGB888_U8
    src_image_size_w : 640
    src_image_size_h : 640
    mean_chn_0 : 0
    mean_chn_1 : 0
    mean_chn_2 : 0
    var_reci_chn_0 : 1.0
    var_reci_chn_1 : 1.0
    var_reci_chn_2 : 1.0
}
```
