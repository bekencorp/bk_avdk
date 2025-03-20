UVC常见问题
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

    在使用uvc时，有些常见的问题，可能会遇到，下面提供一些调试方法。

    Q：调用uvc open的接口后，摄像头不枚举（没有提示连接成功）

    A：这种问题一般是摄像头没有上电，或者供电不足，一半建议给USB供5.0v的电压。BK7258控制USB的LDO是通过GPIO28，拉高即可使能USB的电源，拉低即给USB掉电，可以通过宏进行配置：

    +-------------------------------------+---------------+-------------------------------------+
    |          marco                      |     value     |           implication               |
    +-------------------------------------+---------------+-------------------------------------+
    | CONFIG_USB_VBAT_CONTROL_GPIO_ID     |     0x1C      |        USB电压使能控制GPIO号        |
    +-------------------------------------+---------------+-------------------------------------+

    Q：枚举成功，但是不出图

    A：这种问题一般需要检查参数是否配置错误，比如此摄像头不支持你配置的分辨率，这就需要你重新配置。
    当分辨率配置错误时，会打印如下log: “uvc_camera_set_param, not support this format or resolution, please check!”。
    如果不是，则需要抓包分析，UVC数据包是否正常。包含包头+有效数据，且有效数据为正常值。

    Q：帧率低于预期

    A：所谓的预期是相对于PC而言，比如配置30fps，实际出来只有一半，或者低了很多，插到电脑上分析一下，
    是不是帧率也同样低，这样可以排除摄像头本身的影响。若不是摄像头本身的问题，则考虑是不是SDK的问题。
    这个需要拉逻分分析，或者用协议分析仪抓包分析。

    Q：图像出现切屏异常，图像不完整

    A：这种情况一般需要检查传输方式ISO/BULK是否有配置错误，然后检查上层解包逻辑是否有误。

    Q：图像出现不正常的光晕和畸变

    A：这个一般考虑摄像头本身的问题，插到PC上分析是否有同样的问题。

    Q：使用UVC进行H264 pipeline编码时，打印“h264_encode_finish_handle 26430-65536, error:1”。

    A：打印该log的原因有两个：

        - 一个是当第一个数字（26430）大于第二个数字（65536）时，说明此时I帧的长度大于frame_buffer的长度（第二个数字）；

        修改方式：将宏CONFIG_H264_FRAME_SIZE配置成一个更大的值；

        - 另一个是“error:”的值为1，表示出来的JPEG图像解码长度不对，说明JPEG图像有填充位，为了防止图像异常，解码器内部做了严格的图像长度检查；

        修改方式：建议修改摄像头固件，保证UVC输出的JPEG图像没有填充位，这种类型的填充位一般在末尾且为0xFF或0x00；
        另一种方法是降低解码器内部的图像长度检查机制，不建议这么做，这样有可能会有JPEG异常图像造成显示花屏，参考如下修改。

::

    //Path      ： bk_idk/middleware/driver/jpeg_dec_driver.c

    bool jpeg_dec_comp_status(uint8_t *src, uint32_t src_size, uint32_t dec_size)
    {
        ...

        if (max > 0 && strip + dec_size == src_size - JPEG_TAIL_SIZE)
        {
            ok = true;
        }
        else if (max > 0 && src_size - dec_size == 3 && src[src_size - 3] == 0x00)
        {
            ok = true;
        }
        else
        {
            //可以将这条log打开，分析具体问题
            LOGD("decoder_error, %u, %u, %u, %u\n", src_size, dec_size, strip, max);
        }
        return ok;
    }

----------

    Q：PC端出图正常，接到板子上不出图，而且log打印“cpu1:uvc_stre:W(8614):uvc_id0:30[227 28KB], uvc_id1: 0[0 0KB], uvc_id2:0[0 0KB], packets:[all:62568, err:0]”;

    A：上面的log中uvc_id后面的值一直为0表示没有出图，err后面不为0，表示usb有数据错误。
    出现该问题一般原因为：

        - 请检查usb连接的稳定性
        - 请检查打开uvc使用的分辨率当前摄像头是否支持
        - 请检查使用的port号是否匹配，当前USB最多支持3个port，对应的port范围为：[1, 3]，使能uvc时确保结构体：media_device_t->port的准确性。