cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
#
# 多槽 OTA 的「三分片」（<name> / <name>_A / <name>_B，各带自己的链接脚本）由 SDK 的
# services.ota_core 模块自动扩展 —— 逻辑全在
#     MySDK/services/ota_core/cmake/ota_slots.cmake
# 本文件**不需要**任何 OTA 槽相关内容（槽几何由基版 .ld 派生，三份 .ld 生成到 build/）。
# 想关掉多槽：set(OTA_MULTISLOT OFF)；想改槽几何/基版 .ld：见该文件头部说明。
set(CMAKE_PROJECT_NAME TP_MDC)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# 全局编译定义（必须在 add_subdirectory(MySDK/library) 之前，mysdk 子目标才继承得到）：
#   UART_DRV_BUF_SIZE —— YMODEM 1K 帧长 1029 字节，默认 256 会被 DMA 覆盖（见 ota_src_uart readme）
add_compile_definitions(
    UART_DRV_BUF_SIZE=1088
)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add mysdk-stm32（由 `python sdk_run.py pull` 同步生成于 MySDK/library/，单源真相）
add_subdirectory(MySDK/library)

# 让 SDK 驱动读取工程侧 board_cfg.h 的功能开关（SDK_BOARD_CFG 触发 SDK 头内 #include "board_cfg.h"）
target_include_directories(mysdk PUBLIC ${CMAKE_SOURCE_DIR}/User)
target_compile_definitions(mysdk PUBLIC SDK_BOARD_CFG)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# ===========================
# User 源代码（App + Tasks 已扁平为 Src/Inc；共享模块经 MySDK 引入）
# ===========================
file(GLOB_RECURSE USER_SOURCES 
    "User/Src/*.c"
)

# ===========================
# 添加到可执行文件
# ===========================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

# ===========================
# 头文件路径（User 业务代码；SDK 头由 MySDK 以 PUBLIC 导出）
# ===========================
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    User/Inc
    User            # board_cfg.h（工程板级绑定，唯一允许 include main.h/usart.h 处）
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # 关键修复：SDK 内含若干 HAL 弱符号覆写（HAL_UART_TxCpltCallback 等），
    # 若作为普通静态库链接，链接器会用 HAL 的弱桩满足引用、把覆写目标裁剪掉，
    # 导致 UART TX 完成回调永远指向空桩 → drv->state 卡 TX_BUSY → 发送永久 -2。
    # 用 --whole-archive 强制把 SDK 所有目标文件纳入链接，覆写得以生效。
    -Wl,--whole-archive
    mysdk            # mysdk-stm32 组件（oop_*/devices/SEGGER_RTT/Modbus）
    -Wl,--no-whole-archive

    # Add user defined libraries
)

# 产物（.hex/.bin）由 MySDK（SDK 载荷根）统一生成 —— 见 MySDK/CMakeLists.txt 末段；
# 本文件不需要这段 objcopy 样板。
