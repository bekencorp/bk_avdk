Camera Debug Info
=================================

:link_to_translation:`zh_CN:[中文]`

1 Function Overview
--------------------
	The main function of this project is that after the hardware is connected to the uvc/dvp camera, the data information of each time after MJPEG decoding can be obtained from the serial port, 
	such as the data format after decoding,whether to rotate and so on

2 Code Path
-------------------------------------
	Demo path:``./projects/peripheral/uvc`` or ``./projects/peripheral/dvp``

3 Compiling Commands
-------------------------------------
	Compile the command：``make bk7258 PROJECT=peripheral/uvc`` or ``make bk7258 PROJECT=peripheral/dvp``

4 Presentation
-------------------------------------
	Select the corresponding compilation command according to the type of camera used, connect the development board camera and download the program, open the serial port and press the reset button to see the information printing.
	The information mainly includes the MJPEG data obtained by the camera and the data information output after MJPEG decoding.

  	The MJPEG data obtained by the camera includes camera type (camera_type), frame frame_id, frme_length, and frame address.
	MJPEG decoded data ppi, rotation Angle, output data format and big/little endian.

	.. figure:: ../../../_static/camera_mjpeg_decode_info.png
		:align: center
		:alt: camera_mjpeg_decode info
		:figclass: align-center

		Figure 1. Output of the serial port