set(incs .)

set(srcs
	app_main.c
)

list(APPEND srcs
	
)

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}" 
	PRIV_REQUIRES bk_init bk_cli multimedia lvgl media_service bk_vfs
)
