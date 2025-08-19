cmake_minimum_required(VERSION 3.15)
project(PEACE_UNPACK LANGUAGES C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# 头文件和源文件列表
file(GLOB INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

# 设置头文件分组（适用于 Visual Studio）
source_group("Header Files" FILES ${INCLUDE})

# 创建可执行文件
add_executable(${PROJECT_NAME})

# 添加源文件和头文件目录
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE} ${INCLUDE})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 查找 Zlib 库
find_package(ZLIB REQUIRED)

# 查找并链接 Zstd 和 Iconv 库
# 使用 pkg-config 是个好主意，但我们让它更健壮
find_package(PkgConfig REQUIRED)

# Zstd 库查找
pkg_check_modules(ZSTD_PC QUIET zstd)
if (NOT ZSTD_PC_FOUND)
    # 如果 pkg-config 没找到，我们尝试手动设置路径
    find_library(ZSTD_LIB NAMES zstd HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib)
    find_path(ZSTD_INCLUDE_DIR NAMES zstd.h HINTS /usr/include /usr/local/include)
    if (ZSTD_LIB AND ZSTD_INCLUDE_DIR)
        set(ZSTD_PC_LIBRARIES ${ZSTD_LIB})
        set(ZSTD_PC_INCLUDE_DIRS ${ZSTD_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "Could not find Zstd library. Please install libzstd-dev.")
    endif()
endif()

# # Iconv 库查找
# pkg_check_modules(ICONV_PC QUIET iconv)
# if (NOT ICONV_PC_FOUND)
#     # 如果 pkg-config 没找到，我们尝试手动设置路径
#     find_library(ICONV_LIB NAMES iconv HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib)
#     find_path(ICONV_INCLUDE_DIR NAMES iconv.h HINTS /usr/include /usr/local/include)
#     if (ICONV_LIB AND ICONV_INCLUDE_DIR)
#         set(ICONV_PC_LIBRARIES ${ICONV_LIB})
#         set(ICONV_PC_INCLUDE_DIRS ${ICONV_INCLUDE_DIR})
#     else()
#         message(FATAL_ERROR "Could not find Iconv library. Please install libicu-dev.")
#     endif()
# endif()


target_link_libraries(${PROJECT_NAME} PUBLIC ZLIB::ZLIB ${ZSTD_PC_LIBRARIES} )
target_include_directories(${PROJECT_NAME} PUBLIC ${ZSTD_PC_INCLUDE_DIRS})