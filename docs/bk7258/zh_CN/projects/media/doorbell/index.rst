Doorbell
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是USB摄像头门锁的一个demo，支持端（BK7258设备）到端（手机APP端）的演示。


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
        * 硬件解码时，图像分辨率的宽需要时32的倍数，高的需要时16的倍数。
        * YUV像素排列分为，平面格式（planar）、打包格式（packed）、半平面格式（semi-planar）。硬件编码的数据，需要是packed格式。
    * MJPEG HW Decoder，在pipeline模式中，由于H264的编码数据，需要基于MJPEG解码再编码。因此，本地显示和图传都会用到这个硬件模块。
        * 关闭的时候，需要注意，显示和图传全部关闭的情况，才能关闭此模块。默认demo已经包含了这个逻辑。
    * MJPEG SW Decoder，同一时间，不会两种解码器同时工作。
        * 一旦图像确认是YUV420或者YUV422后，就决定了使用软件解码还是硬件解码。
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
    #define IMAGE_MAX_WIDTH				(864)
    #define IMAGE_MAX_HEIGHT			(480)

    //启动缩放模块时需要关注这两组参数。默认建议，宽度需要比屏大一点。
    #define DISPLAY_MAX_WIDTH			(864)
    #define DISPLAY_MAX_HEIGHT			(480)

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
    如果您没有云账号权限，可以使用debug模式，设置局域网TCP图传方式。


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
    //Loaction  :  CPU0

    int doorbell_camera_turn_on(camera_parameters_t *parameters)
    {
        ...

        //打开UVC摄像头
        ret = media_app_camera_open(&device);

        //设置本地显示旋转。
        //需要注意的是：
        //    1.MJPEG是YUV422 MJPEG时，仅本地显示会旋转。即，H264图像不会旋转。
        //    2.MJPEG是YUV420 MJPEG时，旋转会在软件解码的时候做。即本地显示和H264编码的图像都是旋转后的数据。
        media_app_pipline_set_rotate(rot_angle);

        //打开H264硬件编码加速器
        ret = media_app_h264_pipeline_open();

        ...
    }


5.1.1.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  :  CPU0

    bk_err_t media_app_camera_open(media_camera_device_t *device)
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

        //通知CPU1，去打开UVC摄像头
        ret = media_send_msg_sync(EVENT_CAM_UVC_OPEN_IND, (uint32_t)device);

        ...
    }

5.1.2 获取一张图像
.................................

5.1.2.1 应用代码
*********************************

::

    //Path      ： components/multimedia/camera/uvc.c
    //Loaction  :  CPU1
    
    bk_err_t bk_uvc_camera_open(media_camera_device_t *device)
    {
        ...
    
        //注册了UVC图像的获取的MJPEG数据回调。
        //如果需要做丢帧处理，可以在这个回调里面去做丢帧处理。
        uvc_camera_config_st->jpeg_cb.push   = frame_buffer_fb_push;

        ...
    }


5.1.2.2 接口代码
*********************************

::

    //Path      ： bk_idk/middleware/driver/camera/uvc_camera.c
    //Loaction  :  CPU1
    static void uvc_camera_eof_handle(uint32_t idx_uvc)
    {
        ...

        //这里是从USB的通过ISO或BULK传输，获取一堆数据流。并进行拆包，组包，最终获取到一帧完整的UVC数据。并回调给应用层。
        uvc_camera_config_ptr->jpeg_cb.push(curr_frame_buffer);

        ...
    }


.. attention::
    这里介绍的是MJPEG图像，在CPU1上如何获取。如果您的应用运行在CPU0上，需要通过mailbox发送到CPU0上使用，并且在使用完毕后，需要回到CPU1取释放。


5.1.3 关闭UVC
.................................

5.1.3.1 应用代码
*********************************

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  :  CPU0

    int doorbell_camera_turn_off(void)
    {
        ...

        //关闭H264编码
        media_app_h264_pipeline_close();

        //关闭UVC摄像头
        media_app_camera_close(UVC_CAMERA);

        ...
    }


5.1.3.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  :  CPU0

    bk_err_t media_app_camera_close(camera_type_t type)
    {
        ...

        //关闭UVC
        ret = media_send_msg_sync(EVENT_CAM_UVC_CLOSE_IND, 0);

        //投票允许关闭CPU1。投票的目的是，确保CPU1不用的时候能够被自动关闭，以达到低功耗的目的。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_EN, PM_POWER_MODULE_STATE_OFF);

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
    //Loaction  :  CPU0

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

        media_app_pipline_set_rotate(rot_angle);

        //打开本地LCD显示
		media_app_lcd_pipeline_open(&lcd_open);

        ...
    }


5.2.1.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  :  CPU0

    bk_err_t media_app_lcd_pipeline_open(void *lcd_open)
    {
        ...

        //
        ret = media_app_lcd_pipeline_disp_open(config);

        //
        ret = media_app_lcd_pipeline_jdec_open();

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

    bk_err_t media_app_lcd_pipeline_jdec_open(void)
    {
        int ret = BK_OK;

        //投票启动CPU1。投票的目的是，确保CPU1不用的时候能过够被自动关闭，以达到低功耗的目的。
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_ON);

        //设置旋转角度
        ret = media_send_msg_sync(EVENT_PIPELINE_SET_ROTATE_IND, jpeg_decode_pipeline_param.rotate);

        //打开显示依赖的旋转，缩放，解码模块
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_JDEC_OPEN_IND, 0);

        return ret;
    }



5.2.2 关闭LCD
.................................

5.2.2.1 应用代码
*********************************

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  :  CPU0

    int doorbell_display_turn_off(void)
    {
        ...

        //关闭本地LCD显示
		media_app_lcd_pipeline_close();

        ...
    }


5.2.2.2 接口代码
*********************************

::

    //Path      ： components/multimedia/app/media_app.c
    //Loaction  :  CPU0

    bk_err_t media_app_lcd_pipeline_close(void)
    {
        ...

        //关闭MJPEG，解码/旋转等功能。
        ret = media_app_lcd_pipeline_jdec_close();

        //关闭显示LCD
        ret = media_app_lcd_pipeline_disp_close();

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
    //Loaction  :  CPU0

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
    //Loaction  :  CPU0

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
    //Loaction  :  CPU0

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
    //Loaction  :  CPU0

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
    //Loaction  :  CPU0

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
        
        //设置图像的数据格式
        pixel_format_t fmt;
    } media_transfer_cb_t;


5.5.1 获取H264图像数据
.................................

::

    //Path      ： components/wifi_transfer/src/wifi_transfer.c
    //Loaction  :  CPU0

    bk_err_t bk_wifi_transfer_frame_open(const media_transfer_cb_t *cb)
    {
        ...

        //提高网络图像传输性能
        bk_wifi_set_wifi_media_mode(true);
        bk_wifi_set_video_quality(WIFI_VIDEO_QUALITY_SD);

        ...

        //注册H264图像数据，获取回调
        ret = media_app_register_read_frame_callback(cb->fmt, wifi_transfer_read_frame_callback);

        ...
    }

5.5.2 打开图像数据图传
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  :  CPU0

    int doorbell_video_transfer_turn_on(void)
    {
        ...
		
        //打开图传
        ret = bk_wifi_transfer_frame_open(db_device_info->camera_transfer_cb);

        ...
    }


5.5.2 关闭图像数据图传
.................................

::

    //Path      ： projects/media/doorbell/main/src/doorbell_devices.c
    //Loaction  :  CPU0

    int doorbell_video_transfer_turn_off(void)
    {
        ...
		
        //关闭图传
        ret = bk_wifi_transfer_frame_close();

        ...
    }