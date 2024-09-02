doorviewer_8M
======================================================

:link_to_translation:`zh_CN:[中文]`

1. Introduction
---------------------------------

This project is a demo of a USB/DVP camera door lock, supporting end-to-end (BK7258 device) to mobile app demonstrations. By default, 8Mbyte PSRAM is used.

1.1 Specifications
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Please refer to `Specifications <../doorviewer/index.html#specifications>`_

1.2 Path
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk source code path>/projects/media/doorviewer_8M


2. Framework diagram
---------------------------------

    Please refer to `Framework diagram <../doorviewer/index.html#framework-diagram>`_

3. Configuration
---------------------------------

    Please refer to `Configuration <../doorviewer/index.html#configuration>`_

3.1 Differences
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    The difference between doorbell_8M and doorbell is that the previous configuration uses 8M PSRAM, while the latter configuration uses 16M PSRAM.
    The basic macro configuration of PSRAM on doorbell_8M project CPU0 is as follows:

    +-------------------------------------+---------------+----------------------------------------------------------------+
    |              marco                  |     value     |           implication                                          |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM                       |       Y       |  Enable PSRAM module                                           |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_AS_SYS_MEMORY         |       Y       |  Enable PSRAM as Heap                                          |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_USE_PSRAM_HEAP_AT_SRAM_OOM  |       N       |  Enable if cannot malloc mem from sram, can malloc from psram  |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_BASE             |  0x60700000   |  The start addres of psram as heap in current cpu(cpu0)        |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_SIZE             |    0x80000    |  The size of psram as heap in current cpu                      |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_CPU0_BASE_ADDER  |  0x60700000   |  The start address of psram as heap                            |
    +-------------------------------------+---------------+----------------------------------------------------------------+

    The basic macro configuration of PSRAM on doorbell_8M project CPU1 is as follows:

    +-------------------------------------+---------------+----------------------------------------------------------------+
    |              marco                  |     value     |           implication                                          |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM                       |       Y       |  Enable PSRAM module                                           |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_AS_SYS_MEMORY         |       Y       |  Enable PSRAM as Heap                                          |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_USE_PSRAM_HEAP_AT_SRAM_OOM  |       N       |  Enable if cannot malloc mem from sram, can malloc from psram  |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_BASE             |  0x60780000   |  The start addres of psram as heap in current cpu(cpu1)        |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_SIZE             |    0x80000    |  The size of psram as heap in current cpu                      |
    +-------------------------------------+---------------+----------------------------------------------------------------+
    |  CONFIG_PSRAM_HEAP_CPU0_BASE_ADDER  |  0x60700000   |  The start address of psram as heap                            |
    +-------------------------------------+---------------+----------------------------------------------------------------+

    From the above analysis, it can be concluded that for 8M psram, 0x60700000(CONFIG-PSRAM_SEAP_CPU0_SASE-ADDER) - 0x6800000 is used as a heap, On both cores, 0x80000 was allocated.

.. warning::
    Please pay attention to whether the project and PSRAM size (model) are compatible when using it. If a 16M PSRAM configuration is used to run on an 8M board, the PSRAM used may exceed the maximum range, which may cause a dump.

4. Demonstration explanation
---------------------------------

    Please visit `APP Usage Document <https://docs.bekencorp.com/arminodoc/bk_app/app/zh_CN/v2.0.1/app_usage/app_usage_guide/index.html#debug>`__

    Demo result: During runtime, UVC, LCD, and AUDIO will be activated. The LCD will display UVC and output JPEG (864X480) images that have been decoded and rotated 90° before being displayed on the LCD (480X854),
    After decoding, the YUV is encoded with H264 and transmitted to the mobile phone for display via WIFI (864X480).

.. hint::
    If you do not have cloud account permissions, you can use debug mode to set the local area network TCP image transmission method.


5. Code explanation
---------------------------------

    Please refer `Code explanation <../doorviewer/index.html#code-explanation>`_