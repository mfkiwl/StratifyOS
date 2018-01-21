
set(CMAKE_VERBOSE_MAKEFILE TRUE CACHE INTERNAL "verbose make")

if( ${SOS_APP_ARCH} STREQUAL armv7-m )
	set(BUILD_NAME "build_release_armv7m" CACHE INTERNAL "build name")
	set(BUILD_DEVICE "armv7m" CACHE INTERNAL "build device")
	set(BUILD_FLOAT ${TOOLCHAIN_FLOAT_DIR_ARMV7M} CACHE INTERNAL "build float")
	set(BUILD_FLOAT_OPTIONS "" CACHE INTERNAL "build float options")
else()
	set(BUILD_NAME "build_release_armv7em_fpu" CACHE INTERNAL "build name")
	set(BUILD_FLOAT "${TOOLCHAIN_FLOAT_DIR_ARMV7EM_FPU}" CACHE INTERNAL "build float dir")
	set(BUILD_DEVICE "armv7em" CACHE INTERNAL "build device")
	set(BUILD_FLOAT_OPTIONS ${TOOLCHAIN_FLOAT_OPTIONS_ARMV7EM_FPU} CACHE INTERNAL "build float options")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BUILD_NAME} CACHE INTERNAL "runtime output")

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(BUILD_LIBRARIES ${SOS_APP_LIBRARIES} api sos_crt CACHE INTERNAL "sos build libraries")
set(BUILD_FLAGS -march=${SOS_APP_ARCH} -D__${BUILD_DEVICE} ${BUILD_FLOAT_OPTIONS} CACHE INTERNAL "sos app build options")
set(LINKER_FLAGS "-L${TOOLCHAIN_LIB_DIR}/${SOS_APP_ARCH}/${BUILD_FLOAT}/. -Wl,-Map,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME}_${SOS_APP_ARCH}.map,--defsym=_app_ram_size=${SOS_APP_RAM_SIZE},--gc -Tldscripts/app.ld -nostdlib -u crt" CACHE INTERNAL "sos app linker options")

add_executable(${SOS_APP_NAME}_${SOS_APP_ARCH}.elf ${SOS_APP_SOURCELIST})
add_custom_target(bin_${SOS_APP_NAME}_${SOS_APP_ARCH} DEPENDS ${SOS_APP_NAME}_${SOS_APP_ARCH}.elf COMMAND ${CMAKE_OBJCOPY} -j .text -j .data -O binary ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME}_${SOS_APP_ARCH}.elf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME})
add_custom_target(asm_${SOS_APP_NAME}_${SOS_APP_ARCH} DEPENDS bin_${SOS_APP_NAME}_${SOS_APP_ARCH} COMMAND ${CMAKE_OBJDUMP} -S -j .text -j .priv_code -j .data -j .bss -j .sysmem -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME}_${SOS_APP_ARCH}.elf > ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME}.S)
add_custom_target(size_${SOS_APP_ARCH} DEPENDS asm_${SOS_APP_NAME}_${SOS_APP_ARCH} COMMAND ${CMAKE_SIZE} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SOS_APP_NAME}_${SOS_APP_ARCH}.elf)
add_custom_target(${SOS_APP_ARCH} ALL DEPENDS size_${SOS_APP_ARCH})

target_link_libraries(${SOS_APP_NAME}_${SOS_APP_ARCH}.elf ${BUILD_LIBRARIES})
set_target_properties(${SOS_APP_NAME}_${SOS_APP_ARCH}.elf PROPERTIES LINK_FLAGS ${LINKER_FLAGS})
target_compile_options(${SOS_APP_NAME}_${SOS_APP_ARCH}.elf PUBLIC ${BUILD_FLAGS})
