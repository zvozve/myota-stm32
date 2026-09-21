cmake_minimum_required(VERSION 3.22)

#
# BL 工程根 CMakeLists（TP_MDC_BL）
# 职责只有一件事：读 OTA 状态区 → 把控制权交给有效槽（见 User/Src/boot_flow.c）。
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
# ⚠ BL 必须用 -Os（Release）：FLASH 只有 32K，Debug(-O0) 的 ota_core+bootloader+RTT 会超。
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME TP_MDC_BL)

# BL 不做多槽：它是引导程序，只需要一份产物。
# （多槽三分片是 APP 的事，由 MySDK 的 services.ota_core 自动扩展。）
set(OTA_MULTISLOT OFF)

# Enable compile command to ease indexing with clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# BL 编译定义（必须在 add_executable 之前，mysdk 子目标才继承得到）：
#   SDK_BOARD_CFG —— SDK 头据此 include User/board_cfg.h（本工程为最小骨架）。
# RTOS/裸机开关不再在 CMake 传宏：SDK 在 board_cfg.h 里读 BOARD_USE_RTOS（BL 填 0）。
add_compile_definitions(SDK_BOARD_CFG)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add mysdk-stm32（由 `python sdk_run.py pull` 同步生成于 MySDK/，单源真相）
add_subdirectory(MySDK)

# 让 SDK 读到工程侧 board_cfg.h
target_include_directories(mysdk PUBLIC ${CMAKE_SOURCE_DIR}/User)
target_compile_definitions(mysdk PUBLIC SDK_BOARD_CFG)

# ===========================
# User 源代码（ota_areas 分区表 + boot_flow 启动编排）
# ===========================
file(GLOB_RECURSE USER_SOURCES "User/Src/*.c")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    User/Inc
    User            # board_cfg.h
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
# 与 app 工程同理：SDK 里的 HAL 弱符号覆写必须整体入链（--whole-archive）
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    -Wl,--whole-archive
    mysdk
    -Wl,--no-whole-archive
)

# 产物（.hex/.bin）由 MySDK（SDK 载荷根）统一生成 —— 见 MySDK/CMakeLists.txt 末段；
# 本文件不需要这段 objcopy 样板。
