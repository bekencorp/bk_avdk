Doorbell
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是USB摄像头门锁的一个demo，支持端（BK7258设备）到端（手机APP端）的演示，且支持多摄的切换，目前支持1（dvp）+2（uvc，需要接hub），默认配置使用16M psram。


1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    * 硬件配置：
        * 核心板，**BK7258_QFN88_9X9_V3.2**
        * 显示转接板，**BK7258_LCD_Interface_V3.0**
        * 麦克小板，**BK_Module_Microphone_V1.1**
        * 喇叭小板，**BK_Module_Speaker_V1.1**
        * PSRAM 8M/16M
    * 支持，UVC
        * 参考外设，**864 * 480** 分辨率的UVC
    * 支持，DVP
        * 参考外设，gc2145，**864 * 480** 分辨率的DVP
    * 支持，UAC
    * 支持，TCP局域网图传
    * 支持，UDP局域网图传
    * 支持，尚云，P2P图传
    * 支持，LCD RGB/MCU I8080显示
        * 参考外设，**ST7701SN**，480 * 854 RGB LCD
        * RGB565/RGB888
    * 支持，硬件/软件旋转
        * 0°，90°，180°，270°
    * 支持，板载喇叭
    * 支持，麦克
    * 支持，MJPEG硬件解码
        * YUV422
    * 支持，MJPEG软件解码
        * YUV420
    * 支持，H264硬件解码
    * 支持，OSD显示
        * ARGB888[PNG]
        * 自定义字体

.. warning::
    请使用参考外设，进行demo工程的熟悉和学习。如果外设规格不一样，代码可能需要重新配置。

1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk源代码路径>/projects/media/doorbell

2. 框架图
---------------------------------



2.1 软件模块架构图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


    如下图所示，BK7258有多个CPU：
        * CPU0，运行WIFI/BLE，作为低功耗CPU。
        * CPU1，运行多媒体，作为多媒体高性能CPU。 

.. figure:: ../../../../_static/doorbell_arch.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 1. software module architecture

..

    * UVC方案中，我们采用pipeline方式，来提高整体性能。
    * UVC摄像头输出的图像可以分为两种，一种是YUV420 MJPEG，一种是YUV422 MJPEG。
        * 软件会自动识别，并使用硬件解码器进行YUV422 MJPEG解码。而YUV420 MJPEG，则采用CPU1和CPU2进行软件解码。
        * 硬件解码时，图像分辨率的宽需要是32的倍数，高的需要是16的倍数。
        * YUV像素排列分为，平面格式（planar）、打包格式（packed）、半平面格式（semi-planar）。硬件编码的数据，需要是packed格式。
    * MJPEG HW Decoder，在pipeline模式中，由于H264的编码数据，需要基于MJPEG解码再编码。因此，本地显示和图传都会用到这个硬件模块。
        * 关闭的时候，需要注意，显示和图传全部关闭的情况，才能关闭此模块。默认demo已经包含了这个逻辑。
    * MJPEG SW Decoder，同一时间，不会两种解码器同时工作。
        * 一旦图像确认是YUV420或者YUV422后，就决定了使用软件解码还是硬件解码。
        * 做摄像头切换时，有时候一个摄像头输出的是YUV422 MJPEG，另一个可能是YUV420 MAJPEG，系统会自动重新识别，并重新配置解码方式，客户不需要额外操作。
    * Rota HW 和Rota SW，同一时间，只会使用一种旋转模块。
        * Rota HW，支持RGB 565的图像输出，支持0°、90°、270°。
        * Rota SW，支持0°、90°、180°、270°。
        * 如果需要使用RGB888输出，或者支持180°，满足其中一个条件，都需要切换成软件解码。
        * 当前Rota HW和Rota SW，如何决策，由SDK软件决定。用户只需要在打开LCD时，将旋转角度和输出图像格式参数，输入给对应接口即可。



2.2 代码模块关系图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    如下图所示，多媒体的接口，都定义在 **media_app.h** 和 **aud_intf.h** 中。

.. figure:: ../../../../_static/doorbell_sw_relationship_diag.png
    :align: center
    :alt: relationship diagram Overview
    :figclass: align-center

    Figure 2. module relationship diagram


3. 配置
---------------------------------

3.1 蓝牙与多媒体内存复用
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    为了进一步节约内存，默认工程中，多媒体的内存编解码内存和蓝牙的内存是复用，主要是采用以下两个宏。
    如果希望并行使用两个模块，可自行关闭。关闭前请确认整体内存是否够用。

    ========================================  ===============  ===============  ===============
    Kconfig                                     CPU             Format            Value    
    ========================================  ===============  ===============  ===============
    CONFIG_BT_REUSE_MEDIA_MEMORY                CPU0 && CPU1    bool                y    
    CONFIG_BT_REUSE_MEDIA_MEM_SIZE              CPU0 && CPU1    hex               0x1B000
    ========================================  ===============  ===============  ===============

    * 为了解决实际使用过程中的内存复用冲突，需要在使用多媒体模块前，检查蓝牙的状态，关闭卸载蓝牙。
    * 如果多媒体模块都已经关闭，想再次使用，需要再重新初始化蓝牙。请参考以下代码。
    * CONFIG_BT_REUSE_MEDIA_MEM_SIZE：取值范围是，基于蓝牙硬件模块的需要最大内存和多媒体硬件编码需要的最大内存的两个值，取一个最大值。
        * 一般蓝牙的硬件内存，需求比较小[实际统计，需要根据编译出来map程序来统计]。因为，一般都按照多媒体硬件的最大内存配置。


3.1.1 卸载蓝牙
.................................

::

    #ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
    #if CONFIG_BLUETOOTH
        bk_bluetooth_deinit();
    #endif
    #endif

3.1.2 初始化蓝牙
.................................

::

    bk_bluetooth_init();


3.2 硬件解码内存配置说明
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    硬件加速器，需要使用一部分内存，这部分内存是根据实际的分辨率来优化。
    默认配置参数，LCD是480 * 854的竖屏，Camera是864 * 480的MJPEG图像。

::

    //Camera的输出分辨率，宽度，建议是32的倍数。当屏和Camera的默认配置小的时候，可以通过修改配置宏来优化内存。
    #define IMAGE_MAX_WIDTH                (864)
    #define IMAGE_MAX_HEIGHT            (480)

    //启动缩放模块时需要关注这两组参数。默认建议，宽度需要比屏大一点。
    #define DISPLAY_MAX_WIDTH            (864)
    #define DISPLAY_MAX_HEIGHT            (480)

    typedef struct {
    #if SUPPORTED_IMAGE_MAX_720P
        uint8_t decoder[DECODE_MAX_PIPELINE_LINE_SIZE * 2];
        uint8_t scale[SCALE_MAX_PIPELINE_LINE_SIZE * 2];
        uint8_t rotate[ROTATE_MAX_PIPELINE_LINE_SIZE * 2];
    #else
        uint8_t decoder[DECODE_MAX_PIPELINE_LINE_SIZE * 2];
        uint8_t rotate[ROTATE_MAX_PIPELINE_LINE_SIZE * 2];
    #endif
    } mux_sram_buffer_t;

    * 如果不需要旋转，旋转部分的内存可以省去。
    * 缩放的分辨率需要注意。缩放后的分辨率，宽和高，都必须是8的倍数。

.. caution::
    当CONFIG_BT_REUSE_MEDIA_MEMORY宏打开时，这部分内存会与蓝牙的硬件内存复用。

4. 演示说明
---------------------------------

    请访问
    `APP使用文档 <https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#debug>`__
    查看。

.. hint::
    如果您没有云账号权限，可以使用debug模式，设置局域网TCP/UDP/CS2图传方式。


5. 代码讲解
---------------------------------

5.1 UVC摄像头
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    已支持的外设，请参考 `支持外设 <../../../support_peripherals/index.html>`_


5.1.1 打开UVC
.................................


5.1.1.1 应用代码
*********************************


::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_camera_turn_on(camera_parameters_t *parameters)
    {
        ...

        //打开UVC摄像头
        ret = media_app_camera_open(&db_device_info->video_handle, &device);

        //设置本地显示旋转。
        //需要注意的是：
        //    1.MJPEG是YUV422 MJPEG时，仅本地显示会旋转。即，H264图像不会旋转。
        //    2.MJPEG是YUV420 MJPEG时，旋转会在软件解码的时候做。即本地显示和H264编码的图像都是旋转后的数据。
        media_app_set_rotate(rot_angle);

        //打开H264硬件编码
        ret = media_app_h264_pipeline_open();

        if (device.type == UVC_CAMERA)
        {
            // uvc摄像头输出JPEG图像，默认打开解码器，对jpeg图像进行按16行解码成YUV图像
            media_app_pipeline_jdec_open();
        }
        else if (device.type == DVP_CAMERA)
        {
            // dvp输出YUV后，使能处理YUV数据的task，方便后续实现LCD显示
            media_app_frame_jdec_open(NULL);
        }

        ...
    }


5.1.1.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0

    bk_err_t media_app_camera_open(camera_handle_t *handle, media_camera_device_t *device)
    {
        ...

        //卸载蓝牙
        #ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
        #if CONFIG_BLUETOOTH
            bk_bluetooth_deinit();
        #endif
        #endif

        //投票启动CPU1。投票的目的是，确保CPU1不用的时候能够被自动关闭，以达到低功耗的目的。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_ON);

        media_device_t media_device = {0};
        media_device.param1 = (uint32_t)handle;
        media_device.param2 = (uint32_t)device;
        //通知CPU1，去打开UVC摄像头
        ret = media_send_msg_sync(EVENT_CAM_UVC_OPEN_IND, (uint32_t)media_device);

        ...
    }

    typedef struct {
        camera_type_t type; // camera type
        uint16_t port;      // camera port index(uvc:[1,3], dvp:0)
        uint16_t format;    // camera output image format, reference image_format_t
        uint16_t width;     // camera output image width
        uint16_t height;    // camera output image height
        uint32_t fps;       // camera output image fps
        media_rotate_t rotate;// reserve
    } media_camera_device_t;

5.1.2 获取一张图像
.................................

5.1.2.1 使能接口
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0

    bk_err_t media_app_register_read_frame_callback(image_format_t fmt, frame_cb_t cb)
    {
        ...

        //cb：注册了图像处理回调函数，回调函数中会传输一帧需要的图像
        //fmt：需要读取的图像格式，参考结构体image_format_t

        ...
    }


5.1.2.2 关闭代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0
    bk_err_t media_app_unregister_read_frame_callback(void)
    {
        ...

        //调用这个接口后，停止读取图像，上面注册的回调函数不再被调用

        ...
    }

.. attention::
    这里介绍的是如何读取图像，通过上面的接口用户可以获取图像，但是在回调函数中不要处理太久，建议回调函数中将图像数据拷贝到客户线程处理，否则会卡住读取图像的task，导致丢帧。


5.1.3 关闭UVC
.................................

5.1.3.1 应用代码
*********************************

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_camera_turn_off(void)
    {
        ...

        //关闭H264编码
        media_app_h264_pipeline_close();

        //关闭pipeline解码功能（可能没有开）
        media_app_pipeline_jdec_close();
        //关闭YUV图像处理功能（可能没有打开）
        media_app_frame_jdec_close();

        //关闭所有打开的camera
        do {
            //获取当前已经打开的camera的句柄
            db_device_info->video_handle = bk_camera_handle_node_pop();
            if (db_device_info->video_handle)
            {
                LOGI("%s, %d, %p\n", __func__, __LINE__, db_device_info->video_handle);
                media_app_camera_close(&db_device_info->video_handle);
            }
            else
            {
                break;
            }
        } while (1);

        ...
    }


5.1.3.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0

    bk_err_t media_app_camera_close(camera_handle_t *handle)
    {
        ...

        //关闭UVC，通过camera的句柄去处理
        ret = media_send_msg_sync(EVENT_CAM_CLOSE_IND, (uint32_t)handle);

        //投票允许关闭CPU1。投票的目的是，确保CPU1不用的时候能够被自动关闭，以达到低功耗的目的，但是要确保当前所有的camera都已经关闭。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_OFF);

        ...
    }

    bk_err_t media_app_pipeline_jdec_open(void)
    {
        ...

        //投票启动CPU1。投票的目的是，确保CPU1不用的时候能过够被自动关闭，以达到低功耗的目的。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_ON);

        //设置解码后输出的YUV图像是否需要旋转
        ret = media_send_msg_sync(EVENT_PIPELINE_SET_ROTATE_IND, jpeg_decode_pipeline_param.rotate);

        //使能pipeline JPEG解码功能
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_JDEC_OPEN_IND, 0);

        ...
    }

.. warning::
        * 所有涉及到多媒体的操作，都需要注意低功耗的要求。即打开设备，必须关闭设备，否则无法让整个系统进入低功耗模式。
        * 涉及到CPU1投票的操作，打开和关闭，必须成对出现，否则会出现CPU1无法关闭，功耗增加的问题。
        * 可以参考低功耗章节


5.2 LCD显示
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    已支持的外设，请参考 `支持外设 <../../../support_peripherals/index.html>`_

5.2.1 打开LCD
.................................

5.2.1.1 应用代码
*********************************


::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_display_turn_on(uint16_t id, uint16_t rotate, uint16_t fmt)
    {
        ...

        //设置显示的像素格式
        if (fmt == 0)
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB565_LE);
        }
        else if (fmt == 1)
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB888);
        }

        //设置旋转的角度。
        switch (rotate)
        {
            case 90:
                rot_angle = ROTATE_90;
                break;
            case 180:
                rot_angle = ROTATE_180;
                break;
            case 270:
                rot_angle = ROTATE_270;
                break;
            case 0:
            default:
                rot_angle = ROTATE_NONE;
                break;
        }

        media_app_set_rotate(rot_angle);

        //打开本地LCD显示
        media_app_lcd_disp_open(&lcd_open);

        ...
    }


5.2.1.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0

    bk_err_t media_app_lcd_disp_open(void *config)
    {
        ...

        //lcd模块投票启动cpu1
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_ON);

        //打开lcd显示
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_DISP_OPEN_IND, (uint32_t)config);

        ...
    }

    bk_err_t media_app_lcd_pipeline_disp_open(void *config)
    {
        ...

        //投票启动CPU1。投票的目的是，确保CPU1不用的时候能够被自动关闭，以达到低功耗的目的。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_ON);

        //通知CPU1打开LCD
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_DISP_OPEN_IND, (uint32_t)ptr);

        ...
    }

5.2.2 关闭LCD
.................................

5.2.2.1 应用代码
*********************************

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_display_turn_off(void)
    {
        ...

        //关闭本地LCD显示
        media_app_lcd_disp_close();

        ...
    }


5.2.2.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  ： CPU0

    bk_err_t media_app_lcd_disp_close(void)
    {
        ...

        //关闭lcd显示功能
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_DISP_CLOSE_IND, 0);

        //投票关闭cpu1
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_OFF);

        ...
    }


5.2.3 OSD显示
.................................

    请参考 `OSD视频叠加 <../../../gui/osd/osd_blend.html>`_


5.3 Audio
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

5.3.1 打开UAC，板载MIC/SPEAKER
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_audio_turn_on(audio_parameters_t *parameters)
    {
        ...

        //启用AEC
           if (parameters->aec == 1)
        {
            aud_voc_setup.aec_enable = true;
        }
        else
        {
            aud_voc_setup.aec_enable = false;
        }


        //设置SPEAKER单端模式
        ud_voc_setup.spk_mode = AUD_DAC_WORK_MODE_SIGNAL_END;

        //启用UAC
        if (parameters->uac == 1)
        {
            aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_UAC;
            aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_UAC;
        }
        else //启动板载MIC和SPEAKER
        {
            aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
            aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_BOARD;
        }

        if (aud_voc_setup.mic_type == AUD_INTF_MIC_TYPE_BOARD && aud_voc_setup.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
            aud_voc_setup.data_type = parameters->rmt_recoder_fmt - 1;
        }

        //设置采样率
        switch (parameters->rmt_recorder_sample_rate)
        {
            case DB_SAMPLE_RARE_8K:
                aud_voc_setup.samp_rate = 8000;
            break;

            case DB_SAMPLE_RARE_16K:
                aud_voc_setup.samp_rate = 16000;
            break;

            default:
                aud_voc_setup.samp_rate = 8000;
            break;
        }

        //注册MIC数据回调
        aud_intf_drv_setup.aud_intf_tx_mic_data = doorbell_udp_voice_send_callback;

        ...
    }

5.3.2 获取上行MIC数据
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    //注册MIC回调
    aud_intf_drv_setup.aud_intf_tx_mic_data = doorbell_udp_voice_send_callback;
    ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);

    int doorbell_udp_voice_send_callback(unsigned char *data, unsigned int len)
    {
        ...
        
        //通常实现的回调是往WIFI方向传输。
        return db_device_info->audio_transfer_cb->send(buffer, len, &retry_cnt);
    }


5.3.3 播放下行SPEAKER数据
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    void doorbell_audio_data_callback(uint8_t *data, uint32_t length)
    {
        ...

        //往SPEAKER送数据
        ret = bk_aud_intf_write_spk_data(data, length);

        ...
    }


5.3.4 AEC/降噪处理
.................................

    请参考 `AEC 调试 <../../../audio_algorithms/aec/index.html>`_


5.3.7 关闭UAC，板载MIC/SPEAKER
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_audio_turn_off(void)
    {
        ...

        bk_aud_intf_voc_stop();
        bk_aud_intf_voc_deinit();
        /* deinit aud_tras task */
        aud_work_mode = AUD_INTF_WORK_MODE_NULL;
        bk_aud_intf_set_mode(aud_work_mode);
        bk_aud_intf_drv_deinit();

        ...
    }


5.4 H264编解码
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    请参考 `H264编码 <../../../video_codec/h264_encoding/index.html>`_


5.5 WIFI传输
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

5.5.1 设置WIFI网络数据传输回调
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_udp_service.c
    //Loaction  ： CPU0

    bk_err_t doorbell_udp_service_init(void)
    {
        ...

        //这里设置了图像和音频数据给WIFI的回调
        doorbell_devices_set_camera_transfer_callback(&doorbell_udp_img_channel);
        doorbell_devices_set_audio_transfer_callback(&doorbell_udp_aud_channel);

        ...
    }

    typedef struct {
        //数据最终发送的回调
        media_transfer_send_cb send;

        //数据发送前的Head和payload打包
        media_transfer_prepare_cb prepare;

        //优化延迟的丢包处理
        media_transfer_drop_check_cb drop_check;

        //获取需要填充的TX数据buffer
        media_transfer_get_tx_buf_cb get_tx_buf;

        //获取需要填充的TX buffer的大小
        media_transfer_get_tx_size_cb get_tx_size;
    } media_transfer_cb_t;


5.5.1 获取H264图像数据
.................................

::

    //Path      ： components/wifi_transfer/src/wifi_transfer.c
    //Loaction  ： CPU0

    bk_err_t bk_wifi_transfer_frame_open(const media_transfer_cb_t *cb, uint16_t img_format)
    {
        ...

        //提高网络图像传输性能
        bk_wifi_set_wifi_media_mode(true);
        bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_SD);

        ...

        //注册图像（如果需要H264，则img_format=IMAGE_H264）数据，获取回调
        ret = media_app_register_read_frame_callback(img_format, wifi_transfer_read_frame_callback);

        ...
    }

5.5.2 打开图像数据图传
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ：  CPU0

    int doorbell_video_transfer_turn_on(void)
    {
        ...

        //打开图传
        if (db_device_info->camera_transfer_cb)
        {
            if (db_device_info->h264_transfer)
            {
                ret = bk_wifi_transfer_frame_open(db_device_info->camera_transfer_cb, IMAGE_H264);
            }
            else
            {
                ret = bk_wifi_transfer_frame_open(db_device_info->camera_transfer_cb, IMAGE_MJPEG);
            }
        }
        else
        {
            LOGE("media_transfer_cb: NULL\n");
        }

        ...
    }


5.5.2 关闭图像数据图传
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  ： CPU0

    int doorbell_video_transfer_turn_off(void)
    {
        ...

        //关闭图传
        ret = bk_wifi_transfer_frame_close();

        ...
    }

5.6 摄像头切换
.................................

::

    //Path      ： projects/media/doorbell/main/src/app_main.c
    //Loaction  ： CPU0

    static void media_app_camera_switch(media_camera_device_t *device)
    {
        os_printf("%s\r\n", __func__);
        bk_err_t ret;

        //判断当前是否已经有摄像头已经在工作
        if (db_device_info->video_handle != NULL) {
            //关闭H264 pipeline编码功能
            ret = media_app_pipeline_h264_close();
            if (ret != BK_OK)
            {
                os_printf("media_app_pipeline_h264_close failed\n");
                return;
            }

            //关闭DVP YUV图像显示功能
            ret = media_app_frame_jdec_close();
            if (ret != BK_OK) {
                os_printf("media_app_frame_jdec_close failed\r\n");
                return;
            }

            //关闭jpegdec pipeline功能（默认包含YUV rotate pipeline功能）
            ret = media_app_pipeline_jdec_close();
            if (ret != BK_OK) {
                os_printf("media_app_pipeline_jdec_close failed\r\n");
                return;
            }

            //关闭当前正在使用的摄像头
            ret = media_app_camera_close(&db_device_info->video_handle);
            if (ret != BK_OK) {
                os_printf("media_app_camera_close failed\r\n");
                return;
            }
        }

        //设置YUV旋转角度
        media_app_set_rotate(ROTATE_90);

        //打开需要切换到新摄像头
        ret = media_app_camera_open(&db_device_info->video_handle, device);
        if (ret != BK_OK) {
            os_printf("media_app_camera_open failed\r\n");
            return;
        }

        if (device->type == DVP_CAMERA) {
            //打开DVP YUV图像显示功能
            ret = media_app_frame_jdec_open(NULL);
            if (ret != BK_OK) {
                os_printf("media_app_frame_jdec_open failed\r\n");
                return;
            }
        } else {
            //打开jpegdec pipeline功能（默认包含旋转）
            ret = media_app_pipeline_jdec_open();
            if (ret != BK_OK) {
                os_printf("media_app_pipeline_jdec_open failed\r\n");
                return;
            }

            if (db_device_info->h264_transfer) {
                //如果使能wifi图传H264功能，且使用的是UVC摄像头，需要打开h264 pipeline功能
                ret = media_app_pipeline_h264_open();
                if (ret != BK_OK)
                {
                    os_printf("media_app_pipeline_h264_open failed\n");
                    return;
                }
            }
        }
    }

5.6.1 摄像头切换接口调用流程
...............................

    1.jpeg（864X480）+wifi图传+LCD旋转显示(480X854):

        - 打开第一个摄像头，假设是DVP，且输出的图像格式为IMAGE_YUV&IMAGE_MJPEG（支持同时输出MJPEG和YUV）， media_app_camera_open()；
        - 打开jpeg图传，默认已经配置好网络端口和通道，调用接口读取jpeg图像，format=IMAGE_MJPEG，media_app_register_read_frame_callback()；
        - 如果需要显示到LCD屏幕上，打开硬件显示功能，media_app_lcd_disp_open()；
        - 如果需要显示到LCD屏幕上，且需要旋转，因为默认DVP支持输出YUV图像，只需要将YUV图像旋转，然后让显示模块显示即可，配置旋转角度，media_app_set_rotate()；
        - 如果需要显示到LCD屏幕上，只需要将YUV（可能是上一步已经旋转好的）图像，打开YUV处理功能，将图像发送给硬件显示，media_app_frame_jdec_open()；
        - 如果需要打开SD卡存储MJPEG图像，当前支持两种模式的存储：
            1）MJPEG单次拍照功能，调用一次存储一帧MJPEG图像，media_app_capture()；当不需要再存储时，关闭存储功能，media_app_storage_close()；
            2）MJPEG一直存储，将摄像头拍摄的每一帧图像都存储到SD卡中，media_app_save_start()；运行暂停存储功能，media_app_save_stop()（存储的task不会关闭，可以重新启动存储），当不需要存储功能时，media_app_storage_close()；

        当切换到另一个摄像头（UVC）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 如果需要显示到LCD屏幕上，关闭YUV图像处理功能，media_app_frame_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 如果需要显示到LCD上，打开JPEG解码和旋转功能，media_app_pipeline_jdec_open()，可能需要设置旋转角度；

        当切换到另一个摄像头（UVC）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 如果需要显示到LCD上，打开JPEG解码和旋转功能，media_app_pipeline_jdec_open()，可能需要设置旋转角度；

        当切换到另一个摄像头（DVP）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 如果需要显示到LCD屏幕上，只需要将YUV（可能是上一步已经旋转好的）图像，打开YUV处理功能，将图像发送给硬件显示，media_app_frame_jdec_open()；

        期间以这样的流程任意切换；

        - 当关闭多媒体功能时，需要把所以调用的功能全部关闭，所有关闭的接口已经做了保护，即使没有打开也可以调用关闭：
            1）如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
            2）如果需要显示到LCD屏幕上，关闭YUV图像处理功能，media_app_frame_jdec_close()；
            3）关闭图传，media_app_unregister_read_frame_callback()；
            4）关闭存储，media_app_storage_close()；
            5）关闭所有打开过的摄像头，可以通过接口，bk_camera_handle_node_pop()去获得已经打开的camera，并调用接口media_app_camera_close()去关闭该摄像头，直到bk_camera_handle_node_pop()获取不到为止；

    2.h264(864X480)+wifi图传+LCD旋转显示（480X854）:

        - 打开第一个摄像头，假设是DVP，且输出的图像格式为IMAGE_YUV&IMAGE_H264（支持同时输出H264和YUV）， media_app_camera_open()；
        - 打开h264图传，默认已经配置好网络端口和通道，调用接口读取jpeg图像，format=IMAGE_H264，media_app_register_read_frame_callback()；
        - 如果需要显示到LCD屏幕上，打开硬件显示功能，media_app_lcd_disp_open()；
        - 如果需要显示到LCD屏幕上，且需要旋转，因为默认DVP支持输出YUV图像，只需要将YUV图像旋转，然后让显示模块显示即可，配置旋转角度，media_app_set_rotate()；
        - 如果需要显示到LCD屏幕上，只需要将YUV（可能是上一步已经旋转好的）图像，打开YUV处理功能，将图像发送给硬件显示，media_app_frame_jdec_open()；
        - 如果需要打开SD卡存储H264图像，当前只支持存储连续h264码流，media_app_save_start()；运行暂停存储功能，media_app_save_stop()（存储的task不会关闭，可以重新启动存储），当不需要存储功能时，media_app_storage_close()；

        当切换到另一个摄像头（UVC）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 如果需要显示到LCD屏幕上，关闭YUV图像处理功能，media_app_frame_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 打开h264编码功能，media_app_h264_pipeline_open();
        - 如果需要显示到LCD上，打开JPEG解码和旋转功能，media_app_pipeline_jdec_open()，可能需要设置旋转角度；

        当切换到另一个摄像头（UVC）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 关闭h264编码功能，media_app_h264_pipeline_close();
        - 如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 打开h264编码功能，media_app_h264_pipeline_open();
        - 如果需要显示到LCD上，打开JPEG解码和旋转功能，media_app_pipeline_jdec_open()，可能需要设置旋转角度；

        当切换到另一个摄像头（DVP）时，先关闭上一个摄像头的流程，然后再启动待切换的摄像头，最好启动其他功能；

        - 关闭h264编码功能，media_app_h264_pipeline_close();
        - 如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
        - 关闭当前摄像头，media_app_camera_close()；
        - 打开另一个摄像头（UVC），media_app_camera_open();
        - 如果需要显示到LCD屏幕上，只需要将YUV（可能是上一步已经旋转好的）图像，打开YUV处理功能，将图像发送给硬件显示，media_app_frame_jdec_open()；

        期间以这样的流程任意切换；

        - 当关闭多媒体功能时，需要把所以调用的功能全部关闭，所有关闭的接口已经做了保护，即使没有打开也可以调用关闭：
            1）关闭h264编码功能，media_app_h264_pipeline_close();
            2）如果需要显示到LCD屏幕上，关闭解码和旋转功能，media_app_pipeline_jdec_close()；
            3）如果需要显示到LCD屏幕上，关闭YUV图像处理功能，media_app_frame_jdec_close()；
            4）关闭图传，media_app_unregister_read_frame_callback()；
            5）关闭存储，media_app_storage_close()；
            6）关闭所有打开过的摄像头，可以通过接口，bk_camera_handle_node_pop()去获得已经打开的camera，并调用接口media_app_camera_close()去关闭该摄像头，直到bk_camera_handle_node_pop()获取不到为止；

.. warning::
        * 所有涉及到多媒体的操作，都需要注意低功耗的要求。即打开设备，必须关闭设备，否则无法让整个系统进入低功耗模式。
        * 涉及到CPU1投票的操作，打开和关闭，必须成对出现，否则会出现CPU1无法关闭，功耗增加的问题。
        * 如果进不了低压或者CPU1不能掉电，可以使用命令行：media_debug 8，查看是否有模块未投票。

6 Doorbell
.......................

    下面流程图简单介绍了，doorbell中video组件的启动流程，camera切换流程，已经关闭流程，涉及功能模块：wifi_transfer，sdcard_storage，lcd_display等。

6.1 启动视频功能
..................

    视频相关的功能包括，整个应用中涉及图像的模块。

.. figure:: ../../../../_static/doorbell_video_open_diag.png
    :align: center
    :alt: video open diagram Overview
    :figclass: align-center

    Figure 3. doorbell video open diagram

6.2 摄像头切换
..................

    如下流程图所示，切换摄像头的过程中，先需要检查当前是否已经有摄像头正在工作，如果有，则需要关闭原来的一些流程，关闭正在运行的摄像头，然后打开新的摄像头，最后重启图像处理的流程。

.. figure:: ../../../../_static/doorbell_camera_switch_diag.png
    :align: center
    :alt: camera switch diagram Overview
    :figclass: align-center

    Figure 4. doorbell camera switch diagram

6.3 关闭视频功能
.....................

    在不使用video相关功能时，需要关闭图传任务、存储图像任务、图像处理任务、屏幕显示任务、关闭所有外设（摄像头/屏幕）。此流程中，默认关闭所有video功能，客户可以根据自身需求关闭特定功能。

.. figure:: ../../../../_static/doorbell_video_close_diag.png
    :align: center
    :alt: video close diagram Overview
    :figclass: align-center

    Figure 5. doorbell video close diagram

7 frame_buffer
..................

    针对多媒体完整的图像数据，都是存储在PSRAM中，并且以"frame_buffer_t"的结构体进行存储。并以流形式的链表管理，具体结构如下：

.. figure:: ../../../../_static/frame_buffer_list.png
    :align: center
    :alt: frame_buffer list diagram Overview
    :figclass: align-center

    Figure 6. frame_buffer stream list diagram