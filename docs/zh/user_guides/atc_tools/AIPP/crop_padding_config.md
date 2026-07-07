# Crop/Padding配置说明

AIPP改变图片尺寸需要遵守如下图中的顺序，即先Crop再Padding，每个操作仅能执行一次。

原图大小为srcImageSizeW、srcImageSizeH的图像经过图像预处理后变为模型预期的dstImageSizeW、dstImageSizeH图像尺寸。

**图 1**  改变图像尺寸
![图示](../figures/change_image_size.png "改变图像尺寸")

> [!NOTE]说明
>
>图中实线框表示当前图片size，虚线框表示经过右侧箭头上的AIPP操作处理后的图片size。

从执行角度看，我们需要在配置文件中指出裁剪的起始位置左上点坐标loadStartPosW、loadStartPosH以及裁剪后的图像大小crop\_size\_w、crop\_size\_h。在padding环节，我们需要指明在裁剪后的图像四周padding的尺寸，即left\_padding\_size、right\_padding\_size、top\_padding\_size和bottom\_padding\_size。而经过图像尺寸改变之后最终图片大小，需要跟模型文件输入的图像大小即 **--input\_shape** 中的宽和高相等。

对于YUV420SP\_U8图片类型，load\_start\_pos\_w、load\_start\_pos\_h参数必须配置为偶数。配置样例如下：

```textproto
aipp_op {
    aipp_mode: static
    input_format: YUV420SP_U8

    src_image_size_w: 320
    src_image_size_h: 240

    crop: true
    load_start_pos_w: 10
    load_start_pos_h: 20
    crop_size_w: 50
    crop_size_h: 60


    padding: true
    left_padding_size: 20
    right_padding_size: 15
    top_padding_size: 20
    bottom_padding_size: 15
    padding_value: 0

}
```
