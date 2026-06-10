cmake_minimum_required(VERSION 3.16)
project(online_chat CXX)

# ---------------------------------------------------------------------------
# 基本编译设置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 未显式指定时默认 Release 构建
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 可执行文件输出到构建目录根，便于运行时定位 ./config、./frontend 等相对路径资源
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 导出 compile_commands.json，方便 IDE / clangd 索引
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# 依赖：Drogon（会一并引入 Trantor、jsoncpp 等传递依赖）
# ---------------------------------------------------------------------------
find_package(Drogon CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# jwt-cpp（header-only JWT 库，FetchContent 自动拉取）
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(jwt-cpp)

# ---------------------------------------------------------------------------
# libcrypt（系统库，提供 crypt() 用于 bcrypt 哈希）
# ---------------------------------------------------------------------------
find_library(CRYPT_LIB crypt)
if(NOT CRYPT_LIB)
    message(FATAL_ERROR "libcrypt not found. Install with: sudo apt-get install libcrypt-dev")
endif()

# ---------------------------------------------------------------------------
# 源码收集：按模块分目录组织，统一递归收集 src 下的所有 .cc
#   src/controllers  HTTP 控制器 + WebSocket 控制器
#   src/filters      JWT 鉴权等过滤器
#   src/services     业务逻辑层
#   src/dao          数据访问层
#   src/utils        通用工具（JWT、bcrypt、统一响应等）
#   src/main.cc      程序入口
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# 让各模块可直接以 src 为根做包含，例如 #include "services/auth_service.h"
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    jwt-cpp::jwt-cpp
    ${CRYPT_LIB}
)
