

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(TOOLCHAIN_DIR)
	message(STATUS "User provided toolchain directory: " ${TOOLCHAIN_DIR})
else()
	if(DEFINED SOS_SDK_PATH)
		set(TOOLCHAIN_DIR ${SOS_SDK_PATH}/Tools/gcc)
		message(STATUS "Using SOS_SDK_PATH defined toolchain directory " ${TOOLCHAIN_DIR})
	elseif(DEFINED ENV{SOS_SDK_PATH})
		set(TOOLCHAIN_DIR $ENV{SOS_SDK_PATH}/Tools/gcc)
		set(SOS_SDK_PATH $ENV{SOS_SDK_PATH})
		message(STATUS "Using environment defined toolchain directory " ${TOOLCHAIN_DIR})
	elseif( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin" )
		set(TOOLCHAIN_DIR "/Applications/StratifyLabs-SDK/Tools/gcc")
		message(STATUS "Darwin provided toolchain directory " ${TOOLCHAIN_DIR})
		set(TOOLCHAIN_EXEC_SUFFIX "")
	elseif( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows" )
		set(TOOLCHAIN_DIR "C:/StratifyLabs-SDK/Tools/gcc")
		message(STATUS "Windows provided toolchain directory " ${TOOLCHAIN_DIR})
		set(TOOLCHAIN_EXEC_SUFFIX ".exe")
	elseif( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux" )
		set(TOOLCHAIN_DIR "/StratifyLabs-SDK/Tools/gcc")
		message(STATUS "Linux provided toolchain directory " ${TOOLCHAIN_DIR})
		set(TOOLCHAIN_EXEC_SUFFIX "")
	endif()
endif()

if( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows" )
  set(CMAKE_MAKE_PROGRAM "${SOS_SDK_PATH}/Tools/gcc/bin/make.exe")
  set(TOOLCHAIN_EXEC_SUFFIX ".exe")
endif()


if(TOOLCHAIN_HOST)               # <--- Use 'BOOST_DIR', not 'DEFINED ${BOOST_DIR}'
	MESSAGE(STATUS "User provided toolchain directory " ${TOOLCHAIN_HOST})
else()
	set(TOOLCHAIN_HOST "arm-none-eabi" CACHE INTERNAL "TOOLCHAIN HOST ARM-NONE-EABI")
	MESSAGE(STATUS "Set toolchain host to: " ${TOOLCHAIN_HOST})
endif()

include(${CMAKE_CURRENT_LIST_DIR}/sos-gcc-toolchain.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/sos-build-flags.cmake)

set(TOOLCHAIN_C_FLAGS ${SOS_BUILD_C_FLAGS})
set(TOOLCHAIN_CXX_FLAGS ${SOS_BUILD_CXX_FLAGS})
set(TOOLCHAIN_ASM_FLAGS ${SOS_BUILD_ASM_FLAGS})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
