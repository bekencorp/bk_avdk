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
            // you can open this log, and  analysis the specific reasons
            LOGD("decoder_error, %u, %u, %u, %u\n", src_size, dec_size, strip, max);
        }
        return ok;
    }

--------

    Q: The image is displayed normally on the PC end, but not on the board,
    and the log prints "cpu1:uvc_stre:W(8614):uvc_id0:30[227 28KB], uvc_id1: 0[0 0KB], uvc_id2:0[0 0KB], packets:[all:62568, err:0]";

    A: In the above log, the seq is always 0 after uvc_idx, indicating that no images are output. The err value is not 0, indicating that there are error data in usb.
    There are two reasons for this log message:

        - Please check the stability of the USB connection.

        - Please verify that the resolution currently in use by the UVC is supported by the camera.

        - Please ensure that the port number being used matches, as the USB currently supports up to 3 ports, with the port range being: [1, 3]. When enabling UVC, make sure the accuracy of the structure media_device_t->port.
