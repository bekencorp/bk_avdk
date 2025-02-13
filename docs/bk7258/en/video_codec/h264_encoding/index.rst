H264 Encoding
========================

:link_to_translation:`zh_CN:[中文]`


1. Function overview
---------------------

	h264 coding is mainly used to encode YUV422 data through hardware, output h264 image data, and compress image data

2. Develop materials
----------------------
	Currently avdk provides a detailed sdk interface, refer to the file path:`.\bk_idk\middleware\driver\h264`

	In order to facilitate the use of customers, re-packaging the coding function, the current only need to switch the camera, and then for the image quality, also provide an api interface to adjust, reference file:.\components\multimedia\app\media_app.c '

3. Interface description
-------------------------------

	Debugging description based on h264 image quality: media_app_set_compression_ratio

	Structure parameter description: ``compress_ratio_t``

	- yuv_mode_t mode:		The current mode to be adjusted. Only JPEG and H264 are supported. Select H264
	- h264_qp_t  h264_qp:	The qp value of h264 encode value
	- uint8_t    enable:	enables compression adjustment, which is supported only in JPEG mode
	- uint16_t   jpeg_up:	Upper limit of data JPEG image size, in byte, valid only in JPEG mode
	- uint16_t   jpeg_low:	Lower limit of data JPEG image size, in byte, valid only in JPEG mode
	- uint16_t   imb_bits:	The size of the I frame macro block encoded by H264, in byte. This parameter is valid only in H264 mode. The larger the value, the smaller the compression rate, the higher the image quality and the larger the encoded output image, range [1, 4095].
	- uint16_t   pmb_bits:	The size of the H264 output P-frame macro block, in byte, is valid only in H264 mode. The larger the value, the smaller the compression rate, the higher the image quality, and the larger the encoded output image, range [1, 4095].

	Structure parameter description: ``h264_qp_t``

	- uint8_t init_qp;		The init qp value,range [0, 51];
	- uint8_t i_min_qp;		The minimum qp of I frame encode, range [0, 51];
	- uint8_t i_max_qp;		The maximum qp of I frame encode, range [0, 51];
	- uint8_t p_min_qp;		The minimum qp of P frame encode, range [0, 51];
	- uint8_t p_max_qp;		The maximum qp of P frame encode, range [0, 51];

.. note::

	In addition to paying attention to the valid range of the above values, it is also necessary to pay attention to the maximum value of the corresponding encoding
	must be greater than the minimum value. The image quality can be adjusted through the above interface. It should be noted that improving the image quality will
	undoubtedly increase the amount of data. At present, the maximum output space of a frame of yuv422 data encoding is 64KB.
	The encoded H264 image output is larger than 64K, and there will be instability.

4. Code gear adjust
---------------------------

	The default SDK provides the configuration of h264 encoded gear adjustment, which is defined by the configuration macro: ``CONFIG_H264_QUALITY_LEVEL``, which is described as follows:

	- Value range: [0, 3], define three gears, the values are 1/2/3, corresponding to the h264 compressed image quality from low to high, the clearer the image is.
	- If CONFIG_H264_QUALITY_LEVEL=0, the default value is used instead of the three-gear parameter. The default value reference path: ``.\bk_idk\middleware\soc\bk7258\hal\h264_default_config.h``, you can change the default value to achieve the desired effect.
	- The default SDK gear is defined in the middle gear, CONFIG_H264_QUALITY_LEVEL=2. You can change this value in project config.

5.Adjustment of P-Frame Count
--------------------------------

	Under the default SDK configuration, the number of P-frames per GOP in H264 encoding is 5, which can be adjusted through the macro: ``CONFIG_H264_P_FRAME_CNT``.
	The valid range is [0, 1023]. Increasing the number of P-frames can lead to a reduction in the overall bitrate of the encoded output.

6.Suggestions for Adjusting Image Quality
-------------------------------------------

	The SDK provides three predefined H264 image quality levels, which can be controlled through the macro: ``CONFIG_H264_QUALITY_LEVEL``.
	Adjusting image quality primarily involves parameters within the structure of: ``compress_ratio_t``.

	Debugging Steps:

	1.Adjust the number of P-frames: Increase the number of P-frames based on the variation in image content to reduce bitrate.

	2.Adjust the frame buffer size: The default is 64K. Adjust it as needed to prevent I-frames from being too large and causing the output to fail. It is recommended to set the CONFIG_H264_FRAME_SIZE to 102400 (100K).

	3.Configure the H264 image quality level: Use CONFIG_H264_QUALITY_LEVEL=0 to use the SDK's original encoding parameters.

	4.Real-time compression rate adjustment: Monitor image quality and bitrate simultaneously, and use a controlled variable method to change one parameter at a time.

	5.Obtain/configure encoding parameters: Use the command-line tool to get encoding parameters. To obtain: media h264 get_config, and to configure: media compress h264 init_qp iframe_max_qp pframe_max_qp num_ibits num_pbits.

	6.Adjust the initial quantization parameter (init_qp): Start from 1 and increase by 5 each time, with a maximum value of no more than 51 until the image does not show obvious macroblock motion.

	7.Adjust the I-frame encoding parameter (num_ibits): Increase this value by 20 each time until the image quality is satisfactory, but be cautious not to increase it too much to avoid increased bitrate, with a suggested maximum value of no more than 200.

	8.Adjust the P-frame encoding parameter (num_pbits): Adjust this similarly, but be cautious not to increase it too much to avoid increased bitrate; a suggested maximum value is no more than 150.

	9.Repeat steps 7-8: Continue adjusting until the best parameters are found.

	10.Adjust the maximum quantization parameters for I-frames and P-frames (iframe_max_qp and pframe_max_qp): Reducing these values can improve image quality, but will also increase bitrate. These should not be set lower than 32.

	11.Adjust the frame buffer size: After achieving the final effect, if the I-frame size does not exceed 64K, you can revert it to the default value CONFIG_H264_FRAME_SIZE=65536 (64K).

	Through these steps, you can optimize the number of P-frames and image quality in the H264 encoding process to meet specific application requirements.
