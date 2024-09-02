Common Problems with DVP
=================================


:link_to_translation:`zh_CN:[中文]`


1. Introduction
---------------------------------

    This section mainly describes the common problems and solutions encountered during debugging and using DVP cameras.

    Q: Unable to recognize camera

    A: Use `dvp <../../projects/peripheral/dvp/index. html>`_ Sample project, testing the adapted sample sensor, gc2145,
    If it cannot be recognized, it indicates that there is an error in the parameter configuration of the newly adapted sensor in dvp_xxx.c.
    The most fundamental issue is the I2C read-write address and CHIP_ID of the new sensor.
    If even gc2145 cannot recognize it, check whether the sensor power supply DVDD and IOVDD meet the protocol requirements,
    whether the GPIO controlling the power on the hardware is also GPIO28, and whether I2C is also corresponding to GPO0 and GPO1.
    In addition, attention should be paid to contact issues, which may also be due to insufficient physical connection, resulting in malfunction.

    Q: Abnormal image output and printing: "Sensor's yuyv data resolution is not right".

    A: The DVP data collected by the main control does not match the configured resolution. The VSync/HSync/PCLK signals can be captured through a logic analyzer.
    The following conditions must be followed: if the resolution configured for the main control is 640X480, then a VSync must contain 480 HSync pulses internally,
    and an HSync must contain 640 * 2=1280 PCLK pulses internally.
    If it does not meet the requirements, it will inevitably result in abnormal graphics.
    This problem may be caused by poor physical contact, and needs to be unplugged and reinstalled.
    Possible data collection anomalies may be caused by inconsistent line sequence with the default BK7258.
    Perhaps due to electromagnetic interference from the board, the sampling of the main control may be inaccurate in PCLK. A pull-up filter capacitor 8-22pf can be connected to PLK.

    Q: Abnormal image output and printing: "sensor FIFO is full".

    A: The main control is receiving DVP data too slowly, causing sensor FIFO overflow. The solution can be tried:
    Reduce frame rate/reduce resolution/increase YUV_BUF hardware module clock (current default JPEG: 120MHz, YUV_BUF:120MHz, H264:120MHz).

    Q: Abnormal image output and printing: "JPEG code rate is slow than sensor data rate".

    A: Indicates that the encoding speed is too slow. The solution can be to try reducing the frame rate/resolution/increasing the clock of the encoding hardware module
    (currently default JPEG: 120MHz H264:120MHz).

    Q: Abnormal image output and printing: "h264 encode error".

    A: Indicates H264 encoding error, which may be caused by the sensor's frame gap time being too low, resulting in abnormal control;
    It is also possible that the previous frame has not been fully encoded before the start of the H264 encoding for the new frame.
    In this case, the software code has already covered and the relevant hardware modules can be reset directly.

    Q: I2C abnormality after switching, camera cannot communicate normally.

    A: This situation usually occurs when other peripherals share a set of I2C with DVP, and this I2C is cut off and used by other peripherals;
    It is recommended to use software I2C to prevent the problem of not working after reuse. Open the software I2C function macro control:

    +--------------------+---------------+------------------------------------------------+
    |     marco          |     value     |                 implication                    |
    +--------------------+---------------+------------------------------------------------+
    | CONFIG_SIM_I2C     |       Y       | Enable software to simulate I2C functionality  |
    +--------------------+---------------+------------------------------------------------+

    Q: Configure to h264/JPEG encoding mode, abnormal printing occurs: "···· size no match ····"

    A: In general, the length of data transferred by DMA is not consistent with the actual encoding length.
    This will result in the software discarding such erroneous frames by default to prevent screen flickering and other issues, and resetting the corresponding module.

    Q: The perspective is not in the center when creating the image.

    A: This problem is caused by configuring the parameters (registers) of DVP. It is recommended to find an engineer from the sensor factory to reconfigure it.
    Currently, the configuration provided by the SDK only guarantees normal image output.