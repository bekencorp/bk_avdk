Doorbell_cs2
======================================================

:link_to_translation:`zh_CN:[中文]`

1. Introduction
---------------------------------

This project is a demo of a USB camera door lock, supporting end-to-end (BK7258 device) to mobile app demonstrations. By default, it supports Shangyun for network transmission.

1.1 Specifications
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Please refer to `Specifications <../../media/doorbell/index.html#specifications>`_

1.2 Path
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk source code path>/projects/thirdparty/doorbell_cs2


2. Framework diagram
---------------------------------

    Please refer to `Framework diagram <../../media/doorbell/index.html#framework-diagram>`_

3. Configuration
---------------------------------

    Please refer to `Configuration <../../media/doorbell/index.html#configuration>`_

3.1 Differences
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    The difference between doorbell_CS2 and doorbell is that one supports not only UDP and TCP image transmission,
    but also CS2 cloud transmission, while the latter only supports UDP/TCP.

    Support the macro configuration of Shangyun on CPU0. The difference in macro configuration between the two projects is as follows:

    +---------------+-------------------------------------+---------------+-------------------------------------+
    | project       |          marco                      |     value     |           implication               |
    +---------------+-------------------------------------+---------------+-------------------------------------+
    | doorbell      | CONFIG_INTEGRATION_DOORBELL_CS2     |       N       | Disable CS2 cloud transmission      |
    +---------------+-------------------------------------+---------------+-------------------------------------+
    | doorbell_CS2  | CONFIG_INTEGRATION_DOORBELL_CS2     |       Y       | Enable CS2 cloud transmission       |
    +---------------+-------------------------------------+---------------+-------------------------------------+

4. Demonstration explanation
---------------------------------

    Please visit `APP Usage Document <https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#debug>`__

    Demo result: During runtime, UVC, LCD, and AUDIO will be activated. The LCD will display UVC and output JPEG (864X480) images that have been decoded and rotated 90 degrees before being displayed on the LCD (480X854),
    After decoding, the YUV is encoded with H264 and transmitted to the mobile phone for display via WIFI (864X480).

.. hint::
    If you do not have cloud account permissions, you can use debug mode to set the local area network TCP image transmission method.


5. Code explanation
---------------------------------

    Please refer to `Code explanation <../../media/doorbell/index.html#code-explanation>`_