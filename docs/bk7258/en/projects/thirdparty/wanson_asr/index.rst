Wanson asr demo
===================================

:link_to_translation:`zh_CN:[中文]`

1 Functional Overview
--------------------------
	Based on the local speech recognition library provided by the third-party company ``Huazhen``, the local offline voice wake-up word and command word recognition functions are realized.

2 Code Path
--------------------
	demo path: ``\components\audio_algorithm\wanson_asr``

	Wanson local speech recognition library (floating point library) path: ``\components\bk_thirdparty\asr\wanson``

	For detailed description of Wanson's local speech recognition API interface, please refer to the source file: ``\components\bk_thirdparty\asr\wanson\include\asr.h``

	DEMO Compilation instruction: ``make bk7258 PROJECT=thirdparty/wanson_asr``

3. Demonstration introduction
-------------------------------------------

	After burning the firmware, the device will run real-time voice recognition function when powered on, and can be verified by saying wake-up words and command words to the mic.

	The supported wake-up words and command words are as follows:

	 - ``Little Bee Steward`` recognizes successfully and prints the log ``xiao feng guan jia`` on the serial port
	 - ``armino`` recognizes successfully and prints the log ``a er mi nuo`` on the serial port
	 - ``Visitor mode`` recognize successful serial port print log ``hui ke mo shi``
	 - ``dining mode`` recognizes successfully and prints the log ``yong can mo shi`` on the serial port
	 - ``Leave mode`` recognize successful serial port print log ``li kai mo shi``
	 - ``home mode`` identification success serial port print log ``hui jia mo shi``

4. wanson asr development guide
------------------------------------------

.. note::
	- 1. The wanson Speech Recognition Library requires the audio stream format to be: mono, 16K sampling rate, and 16bit bit width.
	- 2. wanson Speech Recognition Library is based on floating-point arithmetic.
	- 3. After modifying the wake word or command word, the ``libasr.a`` library needs to be replaced.

The process of developing real-time offline recognition based on the wanson speech recognition library is as follows:

    - 1. Initialize speech recognition
    - 2. Initialize audio sampling
    - 3. Run speech recognition
    - 4. Turn on audio sampling

	Examples of interface calls are as follows:

::

    /* init wanson asr lib */
    Wanson_ASR_Init()
    //reset wanson asr
    Wanson_ASR_Reset();

    /* init mic record */
    aud_intf_drv_setup.aud_intf_tx_mic_data = aud_asr_handle;
    //init audio component
    bk_aud_intf_drv_init(&aud_intf_drv_setup);
    aud_work_mode = AUD_INTF_WORK_MODE_GENERAL;
    //set audio component work mode
    bk_aud_intf_set_mode(aud_work_mode);
    //init audio mic
    aud_intf_mic_setup.samp_rate = AUD_ADC_SAMP_RATE_16K;
    ret = bk_aud_intf_mic_init(&aud_intf_mic_setup);

    /* start Speech Recognition */
    //Continuously send the collected data to the algorithm for recognition
    Wanson_ASR_Recog((short*)asr_buff, 480, &text, &score);

    /* turn on audio sampling */
    bk_aud_intf_mic_start();


5. Shanghai Huazhen Electronic Technology Co., Ltd.
-----------------------------------------------------------
     | Official website: http://www.wanson.cn/
     | Headquarters Address: Room 307-308, Huigaoguang Innovation Park, No. 789 Shenwang Road, Minhang District, Shanghai
	 | Shenzhen Office Address: Room 2215-16, East Block, Building 1A, Huiyi City One Center, Xixiang, Baoan District, Shenzhen
     | Tel: 021-61557858
     | Mobile: 13524859176
     |         13296017858
