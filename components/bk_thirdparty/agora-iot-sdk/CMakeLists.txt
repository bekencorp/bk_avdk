if (CONFIG_AGORA_IOT_SDK)

armino_build_get_property(armino_soc ARMINO_SOC)

set(incs include/${ARMINO_SOC})

endif()

armino_component_register(INCLUDE_DIRS "${incs}")

if (CONFIG_AGORA_IOT_SDK)
	file(GLOB agora_libs ${CMAKE_CURRENT_SOURCE_DIR}/${ARMINO_SOC}/libs/*.a)
	foreach(lib ${agora_libs})
		get_filename_component(lib_name ${lib} NAME)
		message(STATUS "agora iot sdk:${lib_name}")
		add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/${ARMINO_SOC}/libs/${lib_name}")
		target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})
	endforeach()
endif()