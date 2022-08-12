# Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

file(GLOB_RECURSE ANJAY_SOURCES "Anjay/src/*.c")
file(GLOB_RECURSE AVS_COMMONS_SOURCES "avs_commons/src/*.c")
file(GLOB_RECURSE AVS_COAP_SOURCES "avs_coap/src/*.c")

add_library(anjay-mbedos
            ${ANJAY_SOURCES}
            ${AVS_COMMONS_SOURCES}
            ${AVS_COAP_SOURCES}
            include/anjay/anjay_config.h
            include/avsystem/coap/avs_coap_config.h
            include/avsystem/commons/avs_commons_config.h
            src/avs_condvar_impl.cpp
            src/avs_init_once_impl.cpp
            src/avs_mbed_hacks.cpp
            src/avs_mbed_hacks.h
            src/avs_mbed_threading_structs.h
            src/avs_mutex_impl.cpp
            src/avs_net_impl/anjay_mbedos_posix_compat.h
            src/avs_net_impl/avs_addrinfo_impl.cpp
            src/avs_net_impl/avs_socket_impl.cpp
            src/avs_net_impl/avs_socket_impl.h
            src/avs_net_impl/avs_tcp_socket_impl.cpp
            src/avs_net_impl/avs_udp_socket_impl.cpp
            src/avs_socket_global.h
            src/avs_time_impl.cpp
            src/mbedtls_timing.c
            src/timing_alt.h)

target_include_directories(anjay-mbedos PRIVATE
                           Anjay/src
                           avs_commons/src
                           avs_coap/src
                           src/avs_net_impl
                           $<TARGET_PROPERTY:mbed-netsocket,INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:mbed-rtos,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(anjay-mbedos PUBLIC
                           Anjay/include_public
                           avs_commons/include_public
                           avs_coap/include_public
                           include
                           src)

target_compile_definitions(anjay-mbedos PRIVATE
                           $<TARGET_PROPERTY:mbed-core,INTERFACE_COMPILE_DEFINITIONS>
                           $<TARGET_PROPERTY:mbed-netsocket,INTERFACE_COMPILE_DEFINITIONS>
                           $<TARGET_PROPERTY:mbed-rtos,INTERFACE_COMPILE_DEFINITIONS>)

target_link_libraries(anjay-mbedos INTERFACE
                      mbed-netsocket)
