#
# Copyright 2019 IBM Corp. and others
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
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.12)

project(Winter)

include(CheckFunctionExists)

enable_testing()

set(CMAKE_CXX_STANDARD 11)

add_subdirectory(third_party/wabt EXCLUDE_FROM_ALL)
target_include_directories(wabt INTERFACE
    # needed because wabt doesn't use `target_include_directories()`
    ${WABT_SOURCE_DIR}
    ${WABT_BINARY_DIR}
    )

add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)

set(WINTER_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

check_function_exists(mremap WINTER_USE_MMAP)
configure_file(${WINTER_SOURCE_DIR}/winter_config.h.in ${Winter_BINARY_DIR}/winter_config.h)

add_library(winter SHARED
    ${WINTER_SOURCE_DIR}/func.cpp
    ${WINTER_SOURCE_DIR}/memory.cpp
    ${WINTER_SOURCE_DIR}/module.cpp
    ${WINTER_SOURCE_DIR}/type.cpp
    ${WINTER_SOURCE_DIR}/wassert.cpp
    )
target_link_libraries(winter PUBLIC wabt)
target_include_directories(winter PUBLIC
    ${WINTER_SOURCE_DIR}
    ${Winter_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/third_party
    )

add_executable(winter-interp ${WINTER_SOURCE_DIR}/main.cpp)
target_link_libraries(winter-interp PUBLIC winter)

add_subdirectory(src/test)
