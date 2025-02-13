UVC
===============================

:link_to_translation:`zh_CN:[中文]`

1. Introduction
---------------------------------

This project is the default debugging project for UVC, which can be used to test whether the inserted UVC works properly. The default is 16M PSRAM.

1.1 Specifications
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Please refer to `Specifications <../../media/doorbell/index.html#specifications>`_

1.2 Path
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk source code path>/projects/peripheral/uvc

2. Framework diagram
---------------------------------

    Please refer to `Framework diagram <../../media/doorbell/index.html#framework-diagram>`_

3. Configuration
---------------------------------

3.1 Bluetooth and Multimedia Memory Reuse
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Using this project is only for verifying UVC, so the memory does not need to be shared with Bluetooth.

    +-------------------------------------+---------------+---------------------------------------------------------------------+
    |          marco                      |     value     |                       implication                                   |
    +-------------------------------------+---------------+---------------------------------------------------------------------+
    | CONFIG_BT_REUSE_MEDIA_MEMORY        |       N       | Multimedia and Bluetooth share one SRAM (time-division multiplexing)|
    +-------------------------------------+---------------+---------------------------------------------------------------------+

3.1.1 Uninstalling Bluetooth
.................................

::

    #ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
    #if CONFIG_BLUETOOTH
        bk_bluetooth_deinit();
    #endif
    #endif

3.1.2 Initialize Bluetooth

.................................

::

    bk_bluetooth_init();

3.2 Differences
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    The difference between UVC and Doorbell is that:
        * UVC starts by default when powered on, and analyzes the output JPEG image raw data format YUV422/YUV420 and output resolution through a software decoder. Other peripherals are not involved and do not support image transfer, LCD display, voice transmission, etc.
        * Doorbell is a complete door lock solution.

4. Demonstration explanation
---------------------------------

    Default power on self start, no additional operation required, Print the following log.

::

    (4736):##MJPEG: frame_id:113, length:56922, frame_addr:0x6020fba0
    video_co:I(4736):4:2:2, YUV:
    (4736):##DECODE:pixel_x:864, pixel_y:480
    (4736):rotate_angle:0(0:0 1:90 2:180 3:270)
    (4736):byte_order:0(0:little endian 1:big endian)
    (4736):out_fmt:YUYV

5. Code explanation
---------------------------------

    Please refer to `Code explanation <../../media/doorbell/index.html#code-explanation>`_