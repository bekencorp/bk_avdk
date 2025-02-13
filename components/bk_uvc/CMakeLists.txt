set(incs include)
set(srcs "")
set(priv_req bk_common)

if (CONFIG_USB_UVC)
    list(APPEND srcs
        src/bk_uvc.c
        src/uvc_urb_list.c
    )

if (CONFIG_STANDARD_DUALSTREAM)
    list(APPEND srcs
        src/bk_uvc_h26x.c
        src/uvc_h26x_urb_list.c
    )
endif()
    
endif()

#list(APPEND priv_req unity include)

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")
