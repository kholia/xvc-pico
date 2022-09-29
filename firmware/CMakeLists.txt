cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE "Release")
set(TINYUSB_DEBUG_LEVEL 0)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(xvcPico C CXX)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(xvcPico xvcPico.c usb_descriptors.c jtag.c axm.c)

target_include_directories(xvcPico PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

pico_set_program_name(xvcPico "xvcPico")
pico_set_program_version(xvcPico "0.1")

#pico_enable_stdio_uart(xvcPico 0)
#pico_enable_stdio_usb(xvcPico 0)

# Add the standard library to the build
target_link_libraries(xvcPico PRIVATE pico_stdlib)

# Add any user requested libraries
target_link_libraries(xvcPico PRIVATE
	pico_unique_id
	tinyusb_device
	tinyusb_board
	pico_multicore
)

pico_add_extra_outputs(xvcPico)
