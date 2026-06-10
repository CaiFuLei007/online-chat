cmake_minimum_required(VERSION 3.16)
project(online_chat_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 依赖 ----
find_package(GTest REQUIRED)
find_package(jsoncpp REQUIRED)

# jwt-cpp（FetchContent 拉取，与主项目保持一致）
include(FetchContent)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(jwt-cpp)

# libcrypt（bcrypt）
find_library(CRYPT_LIB crypt)
if(NOT CRYPT_LIB)
    message(FATAL_ERROR "libcrypt not found")
endif()

# ---- 测试源文件 ----
add_executable(oc_tests
    test_errors.cpp
    test_config.cpp
    test_ws_protocol.cpp
    test_smtp_util.cpp
    test_jwt.cpp
    test_password.cpp
    test_ws_gateway.cpp
)

target_include_directories(oc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Trantor（Drogon 的网络库，提供 InetAddress 等）
find_package(Trantor CONFIG QUIET)
if(NOT Trantor_FOUND)
    # Trantor 通常由 Drogon 安装时一并安装
    find_library(TRANTOR_LIB trantor)
    find_library(TRANTOR_NET_LIB trantor_net)
endif()

target_link_libraries(oc_tests PRIVATE
    GTest::GTest
    GTest::Main
    jsoncpp_lib
    jwt-cpp::jwt-cpp
    ${CRYPT_LIB}
)

# 链接 Trantor（ws_gateway 测试需要 InetAddress）
if(Trantor_FOUND)
    target_link_libraries(oc_tests PRIVATE Trantor::Trantor)
elseif(TRANTOR_LIB)
    target_link_libraries(oc_tests PRIVATE ${TRANTOR_LIB})
endif()

enable_testing()
add_test(NAME oc_tests COMMAND oc_tests)
