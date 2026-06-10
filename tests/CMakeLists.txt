cmake_minimum_required(VERSION 3.16)
project(online_chat_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 依赖 ----
find_package(GTest REQUIRED)
find_package(jsoncpp REQUIRED)

# ---- 测试源文件 ----
# 注意：这些测试只测纯逻辑头文件，不链接 Drogon
add_executable(oc_tests
    test_errors.cpp
    test_config.cpp
    test_ws_protocol.cpp
    test_smtp_util.cpp
)

target_include_directories(oc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(oc_tests PRIVATE
    GTest::GTest
    GTest::Main
    jsoncpp_lib
)

enable_testing()
add_test(NAME oc_tests COMMAND oc_tests)
