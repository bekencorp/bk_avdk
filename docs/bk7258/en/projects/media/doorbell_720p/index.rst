Doorbell_720p
======================================================

:link_to_translation:`zh_CN:[中文]`

1. Introduction
---------------------------------

This project is a demo of a USB camera door lock, supporting end-to-end (BK7258 device) to mobile app demonstrations. The default PSRAM used is 8Mbyte.
Support uvc output 1280X720 image transfer to mobilephone, and scale display to 480X854 lcd screen.

1.1 Specifications
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Please refer to `Specifications <../doorbell/index.html#specifications>`_

1.2 Path
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk source code path>/projects/media/doorbell_720p


2. Framework diagram
---------------------------------

    Please refer to `Framework diagram <../doorbell/index.html#framework-diagram>`_

3. Configuration
---------------------------------

    Please refer to `Configuration <../doorbell/index.html#configuration>`_

3.1 Differences
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    The difference between doorbell_720p and doorbell is that:
        * the previous configuration uvc output 1280X720 mjpeg image, while the latter configuration uvc output 864X480 mjpeg image.

    Because the multimedia module runs on CPU1, the macro configurations of the two projects differ as follows:

    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | project       |          marco                      |     value     |           implication                                              |
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell      | CONFIG_BT_REUSE_MEDIA_MEMORY        |       Y       |Multimedia and Bluetooth share one SRAM (time-division multiplexing)|
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell_720p | CONFIG_BT_REUSE_MEDIA_MEMORY        |       Y       |Multimedia and Bluetooth share one SRAM (time-division multiplexing)|
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell      | CONFIG_BT_REUSE_MEDIA_MEM_SIZE      |    0x1B000    |Multimedia and Bluetooth share the same SRAM size                   |
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell_720p | CONFIG_BT_REUSE_MEDIA_MEM_SIZE      |    0x2F000    |Multimedia and Bluetooth share the same SRAM size                   |
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell      | CONFIG_SUPPORTED_IMAGE_MAX_720P     |       N       |Does not support maximum image resolution of 1280X720               |
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+
    | doorbell_720p | CONFIG_SUPPORTED_IMAGE_MAX_720P     |       Y       |Support maximum image resolution of 1280X720                        |
    +---------------+-------------------------------------+---------------+--------------------------------------------------------------------+

4. Demonstration explanation
---------------------------------

    Please visit `APP Usage Document <https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#debug>`__

    Demo result: During runtime, UVC, LCD, and AUDIO will be activated. The LCD will display UVC and output JPEG (1280X720) images that have been decoded and rotated 90° before being displayed on the LCD (480X854),
    After decoding, the YUV is encoded with H264 and transmitted to the mobile phone for display via WIFI (1280X720).

.. hint::
    If you do not have cloud account permissions, you can use debug mode to set the local area network TCP image transmission method.


5. Code explanation
---------------------------------

    Please refer to `Code explanation <../doorbell/index.html#code-explanation>`_