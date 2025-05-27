set(incs .
		include
	)

set(srcs
		app_main.c
		cli_jpeg_sw_enc.c
	)

set(priv_req bk_init bk_cli)

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
