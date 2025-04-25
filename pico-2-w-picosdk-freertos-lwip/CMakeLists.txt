# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
include(pico-sdk/pico_sdk_init.cmake)
include(FreeRTOS_Kernel_import.cmake)
add_subdirectory(pico-sdk/lib/mbedtls)
project(firmware)
pico_sdk_init()

add_executable(firmware
	main.c
	mongoose.c
	net.c
	packed_fs.c
)

target_include_directories(firmware PUBLIC
  .
)

target_link_libraries(firmware
	pico_stdlib
	pico_rand
	pico_cyw43_arch_lwip_sys_freertos
	FreeRTOS-Kernel-Heap4
	mbedtls
	mbedcrypto
	mbedx509
)
pico_add_extra_outputs(firmware)     # create map/bin/hex file etc.

pico_enable_stdio_usb(firmware 1)    # Route stdio
pico_enable_stdio_uart(firmware 0)   # to USB

# Prevent extreme optimization from breaking OTA
target_compile_options(firmware PRIVATE -fno-tree-loop-distribute-patterns -DENABLE_TESTING=Off)

# Mongoose build flags in mongoose_config.h

# Example build options
add_definitions(-DHTTP_URL="http://0.0.0.0/")
