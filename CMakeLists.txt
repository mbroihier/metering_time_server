set(PROJECT time_server)
cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD pico_w)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PROJECT} C CXX)
pico_sdk_init()
add_executable(${PROJECT} ${PROJECT}.cc
			  ntp_util.cc
                          udp_client_server.cc
                          P2303_Driver.cc
			  TinyGPS.cc)
target_compile_definitions(${PROJECT} PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        UDP_PORT=${UDP_PORT}
        BACKGROUND=1
        )
add_executable("gpssim" gpssim.cc)
target_include_directories(${PROJECT} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)
target_link_libraries(${PROJECT} pico_stdlib
				 pico_cyw43_arch_lwip_poll
                                 pico_multicore
                                 tinyusb_host
                                 tinyusb_board
				 hardware_rtc)
target_link_libraries(gpssim pico_stdlib pico_cyw43_arch_none)
pico_add_extra_outputs(${PROJECT})
#pico_enable_stdio_usb(${PROJECT} 1)  did this when I added P2303_Driver
#pico_enable_stdio_uart(${PROJECT} 0)
pico_add_extra_outputs(gpssim)
pico_enable_stdio_usb(gpssim 1)
pico_enable_stdio_uart(gpssim 0)

