# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set_property(GLOBAL APPEND PROPERTY HEX_FILES_TO_MERGE
      ${app_provision_hex})

set(DTS_ROOT_BINDINGS "${CMAKE_CURRENT_SOURCE_DIR}")

set(BOARD nrf52840dk_nrf52840)

find_package(Zephyr)
project(central)

include_directories(app PRIVATE inc)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE
  ${app_sources}
  )

# zephyr_compile_options(-save-temps)
