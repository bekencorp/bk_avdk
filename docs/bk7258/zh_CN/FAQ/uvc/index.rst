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

    Q：出现异常log: “bk_video_camera_packet_malloc, index:x [x,x,x,x]”，或同步出图异常切屏。

    A：出现该问题的原因是，usb申请packet存储fifo里面的数据，但是申请不到。申请不到的原因是，所有的packet都被用完了，没有空闲态的packet。出现的根本原因有两个：

        * 一个是消耗（解析）已经存储fifo数据的packet效率太低了，或者被卡住了，导致所有的空闲的packet的被消耗完，packet池中都是待解析的packet。
            参考代码：”./bk_idk/middleware/driver/uvc_camera.c”, 解析函数：uvc_camera_process_packet_handler()，需要分析此解析函数是否被卡住了。

        * 另一个原因是出现packet泄漏，泄漏的原因是uvc_class运行在cpu1上，uvc_driver运行在cpu0或者cpu2上，那么两者交互就需要通过mailbox，
            一旦mailbox操作异常，就可能出现packet泄漏。参考异常log：” [=] bk_usb_mailbox_video_camera_packet_push WAIT TIMEOUT”。
            解决方案，出现上面的问题，可以在申请不到空闲packet时，重置packet池中packet的状态。参考如下代码：

    下面的宏定义了packet池的数量。

    +-------------------------------------+---------------+-------------------------------------+
    |          marco                      |     value     |           implication               |
    +-------------------------------------+---------------+-------------------------------------+
    | CONFIG_UVC_MAX_PACKET_CNT           |      16       |     packet的数量取值范围[0, 64]     |
    +-------------------------------------+---------------+-------------------------------------+


::

    //Path      ： bk_idk/middleware/driver/camera/video_common_driver.c

    camera_packet_t *bk_video_camera_packet_malloc(void)
    {
        //申请一包空闲的packet存储uvc fifo数据
        ...

        //当申请不到空闲packet时，会覆盖其他状态的packet
        if (i == list_cnt)
        {
             for (i = list_cnt; i > 0; i--)
             {
                if (camera_packet_list[i - 1]->packet_state == CAM_STREAM_READY)
                {
                    LOGW("%s, index:%d [%d,%d,%d,%d]\r\n", __func__, i - 1,
                            camera_packet_list[0]->packet_state,
                            camera_packet_list[1]->packet_state,
                            camera_packet_list[2]->packet_state,
                            camera_packet_list[3]->packet_state);
                    camera_packet_list[i - 1]->packet_state = CAM_STREAM_IDLE;
                    i--;
                    break;
                }
             }
        }

        ...
    }

----------

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
            LOGD("decoder_error, %u, %u, %u, %u\n", src_size, dec_size, strip, max);
        }
        return ok;
    }

----------

    Q：PC端出图正常，接到板子上不出图，而且log打印“uvc_drv:W(18012):seq:0 ok:39654, error:0, emp:70614, eof_not_ffd9:0, end_not_ffd9:xxxx”;

    A：上面的log中seq一直为0表示没有出图，ok不为0且一直增加表示USB有正常传输数据，end_not_ffd9不为0且一直增加，表示解析包异常。
    出现该问题一般原因为：

        - 如果是BULK传输，可能是中间有传输空包（只含有包头，没有有效数据），当前SDK默认中间不允许传输空包，

        修改方式：建议修改固件，保证不传输空包；也可以将宏CONFIG_UVC_CHECK_BULK_JPEG_HEADER关闭；

        - 如果是ISO传输，可能是0xFFD9末尾有多余的填充数据；

        修改方式：将严格检查图像末尾最后两个字节为0xff/0xd9，改为检查图像最后1024个（或者其他）是否包含0xffd9；后续SDK会这样修改
        检查的长度由宏：CONFIG_JPEG_FRAME_CHECK_LENGTH控制，默认值为1024，可自行修改此值。
        注意：如果图像末尾填充数据太多，会导致检查机制花费的时间更多。