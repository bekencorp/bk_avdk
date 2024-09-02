Double-Board Intertransfer Mission (Server)
==============================================

:link_to_translation:`zh_CN:[中文]`

1 Function Overview
---------------------------------------------------------------------------------------------------------------------------

    This project demonstrates the functionality of unidirectional video transmission and bidirectional audio transmission between two BK7258 boards;
    the server side does not connect to an actual camera (dvp/uvc) and supports the current board to connect to an LCD screen for real-time display of the camera images transmitted from the client side.


1.1 Specification
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	* Hardware Configuration:
		* Core board: **BK7258_QFN88_9X9_V3.2**
		* Display adapter board: **BK7258_LCD_Interface_V3.0**
		* Microphone module board: **BK_Module_Microphone_V1.1**
		* Speaker module board: **BK_Module_Speaker_V1.1**
		* PSRAM: 8M/16M
	* Support, TCP LAN Image transmission
	* Support, UDP LAN Image transmission
	* Support, LCD RGB/MCU I8080 display
		* Reference peripheral: **ST7701SN**, 480 * 854 RGB LCD
		* RGB565/RGB888
	* Support, Hardware rotation
		* 0°, 90°, 180°, 270°
	* Support, Onboard speaker
	* Support, Onboard microphone
	* Support, MJPEG Hardware Decoding
		* YUV422
	* Support, MJPEG Software Decoding
		* YUV420

2. Frame Diagram
---------------------------------

2.1 Software Module Architecture Diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    As shown in the following figure, av_client is responsible for collecting images from the USB camera and transmitting them to av_server via Wi-Fi.
    At the same time, the av_client's local lcd displays the collected images, while av_server displays the images transmitted by av_client.

    * CPU0 runs Wi-Fi/BLE and serves as the low-power consumption CPU.

    * CPU1 runs multimedia and serves as the high-performance multimedia CPU.

.. figure:: ../../../../_static/av_architecture.png
    :align: center
    :alt: module architecture Overview
    :figclass: align-center

    Figure 1. software module architecture

2.2 Code Module Relationship Diagram
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    As shown in the figure below, the interfaces for multimedia are all defined in **media_app.h** and **aud_intf.h**.

.. figure:: ../../../../_static/av_server_framework.png
    :align: center
    :alt: relationship diagram Overview
    :figclass: align-center

    Figure 2. module relationship diagram

3. Configuration
---------------------------------

3.1 Configuration of Image Transmission Mode
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Under the engineering path at config/bk7258/config, modify the macros to configure the image transmission mode; the current default transmission mode is UDP.

    //Transfer using UDP connections
    CONFIG_AV_DEMO_MODE_UDP=y
    CONFIG_AV_DEMO_MODE_TCP=n

    //Transfer using TCP connections
    CONFIG_AV_DEMO_MODE_UDP=n
    CONFIG_AV_DEMO_MODE_TCP=y

4. Demonstration Instructions
---------------------------------

    Two development boards are required for the two-board transmission, with av_client downloaded on Board A and av_server on Board B;

    Board B acts as an AP, with a fixed SSID of "av_demo", no password, and needs to be powered on in front of Board A; it requires connection to the LCD.

    Board A acts as a STA, fixedly connected to the SSID "av_demo"; it requires connection to the USB camera and LCD.

    After powering on Board A, it defaults to connecting to the WiFi on Board B.
    After successful connection, the camera and LCD on Board A are turned on.
    Once Board A is successfully connected, the LCD on Board B displays the image output from the USB on Board A.

5. Code Explanation
---------------------------------

5.1 LCD Display
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Supported Peripherals, please refer to `Supported Peripherals <../../../support_peripherals/index.html>`_

5.1.1 Open LCD
.................................

5.1.1.1 Application code
*********************************


::

    //Path      :  projects/media/av_server/main/src/av_server_devices.c
    //Loaction  :  CPU0

    int av_server_display_turn_on(uint16_t id, uint16_t rotate, uint16_t fmt)
    {
        ...

        //Set the pixel format for display
        if (fmt == 0)
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB565_LE);
        }
        else if (fmt == 1)
        {
            media_app_lcd_fmt(PIXEL_FMT_RGB888);
        }

        //Set the rotation angle
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

        //Open the local LCD display
		media_app_lcd_pipeline_open(&lcd_open);

        ...
    }


5.1.1.2 Interface Code
*********************************

::

    //Path      :  components/multimedia/app/media_app.c
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

        //Vote to start CPU1. The purpose of the vote is to ensure that CPU1 can be automatically turned off when it is not in use, in order to achieve the goal of low power consumption.
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_LCD, PM_POWER_MODULE_STATE_ON);

        //Notify CPU1 to turn on the LCD.
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_DISP_OPEN_IND, (uint32_t)ptr);

        ...
    }

    bk_err_t media_app_lcd_pipeline_jdec_open(void)
    {
        int ret = BK_OK;

        //Vote to start CPU1. The purpose of the vote is to ensure that CPU1 can be automatically turned off when it is not in use, in order to achieve the goal of low power consumption.
        bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_VIDP_JPEG_DE, PM_POWER_MODULE_STATE_ON);

        //Set the rotation angle
        ret = media_send_msg_sync(EVENT_PIPELINE_SET_ROTATE_IND, jpeg_decode_pipeline_param.rotate);

        //Turn on rotation, scale, and decode modules.
        ret = media_send_msg_sync(EVENT_PIPELINE_LCD_JDEC_OPEN_IND, 0);

        return ret;
    }

5.2 Audio
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

5.2.1 Open UAC or Onboard MIC/SPEAKER
.......................................

::

    //Path      :  projects/media/av_client/main/src/av_server_devices.c
    //Loaction  :  CPU0

    int av_server_audio_turn_on(audio_parameters_t *parameters)
    {
        ...

        //Enable AEC
        if (parameters->aec == 1)
        {
            aud_voc_setup.aec_enable = true;
        }
        else
        {
            aud_voc_setup.aec_enable = false;
        }


        //Set the SPEAKER to single-ended mode.
        ud_voc_setup.spk_mode = AUD_DAC_WORK_MODE_SIGNAL_END;

        //Enable UAC
        if (parameters->uac == 1)
        {
            aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_UAC;
            aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_UAC;
        }
        else //Enable Onboard MIC and SPEAKER
        {
            aud_voc_setup.mic_type = AUD_INTF_MIC_TYPE_BOARD;
            aud_voc_setup.spk_type = AUD_INTF_SPK_TYPE_BOARD;
        }

        if (aud_voc_setup.mic_type == AUD_INTF_MIC_TYPE_BOARD && aud_voc_setup.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
            aud_voc_setup.data_type = parameters->rmt_recoder_fmt - 1;
        }

        //Set the sampling rate
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

        //Register MIC data callback
        aud_intf_drv_setup.aud_intf_tx_mic_data = av_client_udp_voice_send_callback;

        ...
    }

5.2.2 Fetch uplink MIC data
.................................

::

    //Path      :  projects/media/av_server/main/src/av_server_devices.c
    //Loaction  :  CPU0

    //Register MIC data callback
	aud_intf_drv_setup.aud_intf_tx_mic_data = av_client_udp_voice_send_callback;
	ret = bk_aud_intf_drv_init(&aud_intf_drv_setup);

    int av_server_udp_voice_send_callback(unsigned char *data, unsigned int len)
    {
        ...

        //The commonly implemented callback typically involves transmitting in the WiFi direction.
        return db_device_info->audio_transfer_cb->send(buffer, len, &retry_cnt);
    }


5.2.3 Play downlink SPEAKER data
.................................

::

    //Path      :  projects/media/av_server/main/src/av_server_devices.c
    //Loaction  :  CPU0

    void av_server_audio_data_callback(uint8_t *data, uint32_t length)
    {
        ...

        //Send data to the SPEAKER.
        ret = bk_aud_intf_write_spk_data(data, length);

        ...
    }

