set(PROJECT metering_time_server)
cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD pico_w)
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PROJECT} C CXX)
pico_sdk_init()
add_executable(${PROJECT} ${PROJECT}.cc
                          dhcpserver.c
                          dnsserver.c
                          Meter.cc
                          Read_Temperature.cc
			  ntp_util.cc
                          udp_client_server.cc
                          TinyGPS++.cc)
target_compile_definitions(${PROJECT} PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        UDP_PORT=${UDP_PORT}
        UDP_PORT2=${UDP_PORT2}
        BACKGROUND=1
        )
target_include_directories(${PROJECT} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)
target_link_libraries(${PROJECT} pico_stdlib
				 pico_cyw43_arch_lwip_poll
                                 pico_multicore
				 hardware_rtc
                                 hardware_adc)
pico_add_extra_outputs(${PROJECT})
pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)

set(PROJECT2 udp_metering_client)
project(${PROJECT2} C CXX)
add_executable(${PROJECT2} ${PROJECT2}.cc
                          udp_client_server.cc
			  )
target_compile_definitions(${PROJECT2} PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
	UDP_PORT=${UDP_PORT}
	UDP_PORT2=${UDP_PORT2}
        DEBUG_ON_PICO=ON
        DCMAKE_BUILD_TYPE=Debug
        )
target_include_directories(${PROJECT2} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)
target_link_libraries(${PROJECT2} pico_stdlib
				 pico_cyw43_arch_lwip_poll
				 pico_multicore
                                 hardware_rtc
                                 hardware_adc
				 )
pico_add_extra_outputs(${PROJECT2})
pico_enable_stdio_usb(${PROJECT2} 1)
pico_enable_stdio_uart(${PROJECT2} 0)

