AEC Debug
=================================

:link_to_translation:`zh_CN:[中文]`

1.Overview
--------------------

    The AEC echo cancellation algorithm is mainly used in bidirectional voice call scenarios, which eliminates the audio played by the speaker collected by the device end mic through the algorithm to obtain clean human voices.

2.Function Overview Engineering Code Path
----------------------------------------------------------

	The reference demo path for using the AEC algorithm is as follows: ``./components/audio_algorithm/aec``

	The demo provides methods for testing and verifying the AEC algorithm's PCM data and interface usage.

3.Debugging of AEC echo cancellation effect
----------------------------------------------------------

	AEC debugging precautions:

.. important::
	The echo situation is closely related to the device's mic sensitivity, mic gain, speaker sensitivity, speaker gain, product casing layout, sound intensity, etc. Different echo situations require different AEC parameters to handle, so before starting to debug AEC parameters, it is necessary to ensure that the following prerequisites are completed:

	- 1. The mic model must be fixed, and the sensitivity of different models may vary. The higher the sensitivity, the greater the echo. Therefore, it is advisable to choose a mic with lower sensitivity while meeting the requirements of the usage scenario.
	- 2. The gain of the mic must be fixed, and the magnitude of the gain will affect the amplitude of the audio signal.
	- 3. The speaker model must be fixed, and the power of different models may vary. The higher the power, the louder the sound, and the greater the echo. Therefore, it is advisable to choose a speaker with lower power while meeting the requirements of the usage scenario.
	- 4. The gain of the speaker (including PA and DAC gain inside the digital chip) must be fixed. The larger the gain, the louder the speaker's volume, and the greater the echo. Therefore, it is possible to lower the gain of the speaker as much as possible while meeting the requirements of the usage scenario.
	- 5. The product casing and sound chamber must be fixed. Different casings and sound chambers can cause different echo conditions of the equipment, so the product casing and sound chamber must be fixed.

The voice call application developed based on the aud-intf component needs to debug the effect of AEC echo cancellation, mainly by adjusting the following parameters: ``echo depth`` , ``max amplitude`` , ``min amplitude`` , ``noise level`` , and ``noise param`` .

The debugging steps are as follows:
	- 1.Ensure that the two devices involved in the call (usually the phone and the board) are in remote mode, ensuring that the sound of the phone's speech is not captured by the board's mic, and then open the voice call;
	- 2.Based on the echo effect heard, use the UART to send a serial command ``aud-intf_set-aec_param_test {param value}`` to set the values of each parameter and debug the AEC parameters online;
	- 3.Send the serial command ``aud_intf_set_aec_param_test ec_depth {value}`` to set the ``echo depth`` . The larger the echo, the greater the value. When the value continues to increase but cannot improve the echo cancellation effect, stop setting the value;
	- 4.Send the serial commands ``aud_intf_set_aec_param_test TxRxThr {value}`` and ``aud_intf_set_aec_param_test TxRxFlr {value}`` based on the range of echo size to set the values of ``max amplitude`` and ``min amplitude`` , optimizing the echo cancellation effect;
	- 5.Send the serial command ``aud_intf_set_aec_param_test ns_level {value}`` based on the size of the background noise heard to set the ``noise level`` to optimize the effect of background noise elimination. The larger the background noise, the larger the value set;
	- 4.Send the serial command ``aud_intf_set_aec_param_test ns_para {value}`` to set the value of ``noise param`` to 0, 1, and 2, respectively, and select the best value;
	- 5.Send the serial command ``aud_intf_get_aec_param_test`` to obtain and record the online debugging results of all parameters, and set them as the default parameters for AEC during voice initialization.

.. note::
 - 1. To ensure the effectiveness of the serial command ``aud-intf_set-aec_param_test {param value}`` , open the macro ``PROFIG-AUD-INTF-TEST=y``.
 - 2. Failure to tune AEC parameters can result in occasional abnormal sounds heard by the opposite end of the board (usually on the mobile phone), such as occasional silence or partial echo.

4.Reference link
----------------------------------------

    `API Reference : <../../api-reference/multi_media/bk_aec.html>`_ Introduced the AEC API interface

    `User and Developer Guide : <../../audio_algorithms/aec/index.html>`_ Introduced common usage scenarios of AEC
