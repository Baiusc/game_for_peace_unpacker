cmake_minimum_required(VERSION 3.15)
project(PEACE_PAK LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# 头文件目录
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 查找 Zlib 和 Zstd 库
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)

# Zstd 库查找
pkg_check_modules(ZSTD_PC QUIET zstd)
if (NOT ZSTD_PC_FOUND)
    find_library(ZSTD_LIB NAMES zstd HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib)
    find_path(ZSTD_INCLUDE_DIR NAMES zstd.h HINTS /usr/include /usr/local/include)
    if (ZSTD_LIB AND ZSTD_INCLUDE_DIR)
        set(ZSTD_PC_LIBRARIES ${ZSTD_LIB})
        set(ZSTD_PC_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "Could not find Zstd library. Please install libzstd-dev.")
    endif()
endif()

# Unpack 项目
add_executable(PEACE_UNPACK)
target_sources(PEACE_UNPACK PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game_for_peace_unpack.c
)
target_link_libraries(PEACE_UNPACK PRIVATE ZLIB::ZLIB ${ZSTD_PC_LIBRARIES})
target_include_directories(PEACE_UNPACK PRIVATE ${ZSTD_PC_INCLUDE_DIRS})

# Repack 项目
add_executable(PEACE_REPACK)
target_sources(PEACE_REPACK PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game_for_peace_repack.c
)
target_link_libraries(PEACE_REPACK PRIVATE ZLIB::ZLIB ${ZSTD_PC_LIBRARIES})
target_include_directories(PEACE_REPACK PRIVATE ${ZSTD_PC_INCLUDE_DIRS})

# 将头文件添加到两个目标
target_include_directories(PEACE_UNPACK PUBLIC ${INCLUDE_DIR})
target_include_directories(PEACE_REPACK PUBLIC ${INCLUDE_DIR})

# 设置源文件分组
source_group("Unpack Sources" FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/game_for_peace_unpack.c)
source_group("Repack Sources" FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/game_for_peace_repack.c)
source_group("Header Files" FILES ${INCLUDE_DIR}/sha1.h)