set(incs .
		include
	)

set(srcs
		app_main.c
	)

if (CONFIG_WENWEN_ASR)
	list(APPEND srcs
		wenwen_asr.c
	)
endif()

set(priv_req bk_init bk_cli avdk_libs multimedia avdk_utils wenwen audio_record)

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
