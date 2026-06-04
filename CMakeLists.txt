cmake_minimum_required(VERSION 3.16)
project(CloudDisk VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 版本宏（解决 MinGW 下 CreateFile2 未定义等问题）
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/database.cpp
    src/file_handler.cpp
    src/util.cpp
    src/crypto.cpp
)

set(HEADERS
    src/database.h
    src/file_handler.h
    src/util.h
    src/crypto.h
)

# 可执行文件
add_executable(cloud_disk ${SOURCES} ${HEADERS})

# 第三方头文件路径
target_include_directories(cloud_disk PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)

# Windows 下链接 ODBC 和 Winsock
if(WIN32)
    target_link_libraries(cloud_disk PRIVATE odbc32 ws2_32 bcrypt)
endif()

# 工作目录设为项目根目录（方便找 storage/）
set_target_properties(cloud_disk PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
