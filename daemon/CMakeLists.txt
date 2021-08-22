# https://github.com/trabucayre/openFPGALoader/blob/master/CMakeLists.txt
cmake_minimum_required(VERSION 3.0)

project(xvcd-pico VERSION "0.0.1" LANGUAGES CXX C)
add_definitions(-DVERSION=\"v${PROJECT_VERSION}\")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS "-O2 -Wall -Wextra ${CMAKE_CXX_FLAGS_DEBUG}")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb -Wall -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

set(XVC_PICO_SOURCE
	xvcpico.c
)

add_executable(xvcd-pico
	${XVC_PICO_SOURCE}
)

include_directories(
	${LIBUSB_INCLUDE_DIRS}
	${LIBFTDI_INCLUDE_DIRS}
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_library(LIBFTDI1STATIC libftdi1.a REQUIRED)
	find_library(LIBUSB1STATIC libusb-1.0.a REQUIRED)
	target_link_libraries(xvcd-pico ${LIBFTDI1STATIC} ${LIBUSB1STATIC})
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework IOKit")
	link_directories(/usr/local/lib)
	target_include_directories(xvc-pico PRIVATE /usr/local/include)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	set_target_properties(xvcd-pico PROPERTIES LINK_SEARCH_END_STATIC 1)
else()
target_link_libraries(xvcd-pico
	${LIBUSB_LIBRARIES}
	${LIBFTDI_LIBRARIES}
)

endif()

install(TARGETS xvcd-pico DESTINATION bin)
