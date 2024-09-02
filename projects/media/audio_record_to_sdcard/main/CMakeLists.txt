set(incs .
		include
	)

set(srcs
		app_main.c
	)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
	audio_record.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
	list(APPEND srcs
		vendor_flash.c
	)
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES media bk_init bk_cli avdk_libs media_service multimedia)
