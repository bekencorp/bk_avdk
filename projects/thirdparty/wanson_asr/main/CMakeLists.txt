set(incs .
		include
	)

set(srcs
		app_main.c
	)


if (CONFIG_WANSON_ASR)
	list(APPEND srcs
        wanson_asr.c
    )
endif()

set(priv_req bk_init bk_cli avdk_libs multimedia audio_record wanson)

if (CONFIG_OVERRIDE_FLASH_PARTITION)
	list(APPEND srcs
		vendor_flash.c
	)
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES "${priv_req}"
)
