Common Problems with UVC
=================================


:link_to_translation:`zh_CN:[中文]`


1. Introduction
---------------------------------

    When using UVC, some common issues may arise, and here are some debugging methods provided.

    Q: After calling the UVC open interface, the camera does not enumerate (no prompt of successful connection)

    A: This kind of problem is usually due to the camera not being powered on or insufficient power supply.
    It is recommended to supply 5.0V of voltage to the USB. The BK7258 controls the USB LDO through GPIO28,
    which can enable the USB power by pulling it high, and can shutdown the USB power by pulling it low.
    This can be configured through macros:

    Q: Enumeration was successful, but no image is displayed.

    A: This issue usually requires checking if the parameters are configured incorrectly, such as the camera not supporting the resolution you have configured,
    which would then require you to reconfigure it. When the resolution is configured incorrectly, you will see the following log: "uvc_camera_set_param,
    not support this format or resolution, please check!". If this is not the case, you may need to capture packets to analyze whether the UVC data packets are normal,
    including headers and valid data, and that the valid data is within the normal range.

    Q: The frame rate is lower than expected.

    A: The "expected" refers to the frame rate in comparison to a PC, for example, if you configure it to 30fps but the actual output is only half that or much lower,
    plug it into the computer to analyze whether the frame rate is also low on the computer, which can help rule out issues with the camera itself. If it's not an issue with the camera itself, then consider whether it's a problem with the SDK. This requires log analysis or packet capture analysis using a protocol analyzer.

    Q: There are abnormal screen cutting issues in the image, and the image is incomplete.

    A: This situation usually requires checking if there are any misconfigurations in the transmission mode, ISO/BULK,
    and then checking if there are any errors in the upper layer unpacking logic.

    Q: The image displays abnormal halos and distortions.

    A: This is generally considered an issue with the camera itself. Try plugging it into a PC to analyze whether the same problem occurs.

    Q: The related to a log message: "bk_video_camera_packet_malloc, index:x [x,x,x,x]" or an abnormal screen switching during image synchronization.

    A: The cause of this issue is that when USB is requesting packets to store in the FIFO (First-In-First-Out) buffer,
    it cannot get them because all the packets are in use, with no free packets available. The underlying reasons are two-fold:

        * The consumption (parsing) efficiency of the packets already stored in the FIFO is too low, or it is stuck, leading to all the free packets being consumed, with the packet pool filled with packets waiting to be parsed. Reference code: " ./bk_idk/middleware/driver/uvc_camera.c", parsing function: uvc_camera_process_packet_handler(), analyze if this parsing function is stuck.

        * Another reason is packet leakage, which can occur when the uvc_class runs on cpu1, while the uvc_driver runs on cpu0 or cpu2. The interaction between these two requires a mailbox, and if there is an abnormal mailbox operation, packet leakage may occur. Reference error log: " [=] bk_usb_mailbox_video_camera_packet_push WAIT TIMEOUT". The solution is that when it's impossible to obtain a free packet, the state of the packets in the packet pool should be reset. Reference code as follows:

    The following macro defines the number of packets in the packet pool. Here is the translation into English:

::

    //Path      : bk_idk/middleware/driver/camera/video_common_driver.c

    camera_packet_t *bk_video_camera_packet_malloc(void)
    {
        // malloc a free packet to save uvc fifo data
        ...

        // while can not malloc free packet, will merge other state packets
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

-------------

    Q: When using UVC for H.264 pipeline encoding, the log "h264_encode_finish_handle 26430-65536, error:1" is printed.

    A: There are two reasons for this log message:

    - One is that when the first number (26430) is greater than the second number (65536), it indicates that the length of the I frame is greater than the length of the frame_buffer (the second number).

      Modification method: Increase the value of the macro CONFIG_H264_FRAME_SIZE to a larger number.

    - The other is that "error:" has a value of 1, indicating that the length of the decoded JPEG image is incorrect. This suggests that the JPEG image has padding bits. To prevent image anomalies, the decoder performs strict length checks on the image internally.

    Modification method: It is recommended to modify the camera firmware to ensure that the UVC output JPEG image does not have padding bits.
    Such padding bits are usually at the end and are either 0xFF or 0x00.

    Another method is to lower the internal length check mechanism of the decoder, which is not recommended as it may lead to JPEG anomalies causing the screen to display incorrectly.
    Reference code modification.

::

    //path        : bk_idk/middleware/driver/jpeg_dec_driver.c

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

--------

    Q: The image is displayed normally on the PC end, but not on the board,
    and the log prints "uvc_drv:W(18012):seq:0 ok:39654, error:0, emp:70614, eof_not_ffd9:0, end_not_ffd9:xxxx";

    A: In the above log, the seq is always 0, indicating that no images are output. The ok value is not 0 and keeps increasing,
    which means that normal data is being transmitted over USB. The end_not_ffd9 value is not 0 and keeps increasing,
    indicating an abnormal packet parsing. There are two reasons for this log message:

        - In BULK transmission, there are empty packets (only containing header but no valid data) in the transmission. The current firmware defaults to not allow the transmission of empty packets.

          Modification method: It is recommended to modify the firmware to ensure no empty packets are transmitted; alternatively, you can disable the macro CONFIG_UVC_CHECK_BULK_JPEG_HEADER.

        - In ISO transmission, there are filling data at the end of frame.

          Modification method: The current firmware defaults strictly check if the last two bytes of the frame are 0xffd9, change to check if the last 1024 bytes of frame contain 0xffd9.
          The SDK will be modified as follows: The length of the check is controlled by the macro: CONFIG_JPEG_FRAME_CHECK_LENGTH, with a default value of 1024,
          which can be modified as needed. Note: If there is an excessive amount of padding data at the end of the image, it will cause the check mechanism to take more time.