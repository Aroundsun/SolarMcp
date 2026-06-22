# SolarMcp 单元测试

set(TEST_SOURCES
    test_logger.cpp
    test_config.cpp
    test_thread_pool.cpp
    test_buffer.cpp
    test_codec.cpp
    test_dispatcher.cpp
    test_tool.cpp
    test_read_file_tool.cpp
    test_plugin.cpp
    test_reactor.cpp
    test_auth.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        solarmcp
        GTest::gtest
        GTest::gtest_main
        pthread
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# test_plugin 需加载真实 .so：导出符号 + 插件构建产物路径
if(TARGET shell_plugin)
    add_dependencies(test_plugin shell_plugin minimal_plugin)
    target_compile_definitions(test_plugin PRIVATE
        "SOLARMCP_PLUGIN_DIR=\"${CMAKE_SOURCE_DIR}/plugins\"")
endif()

# shell 工具在 plugins/ 目录，单独编译其源文件
add_executable(test_shell_tool
    test_shell_tool.cpp
    ${CMAKE_SOURCE_DIR}/plugins/shell/shell_tool.cpp
)
target_include_directories(test_shell_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(test_shell_tool PRIVATE
    solarmcp
    GTest::gtest
    GTest::gtest_main
    pthread
)
add_test(NAME test_shell_tool COMMAND test_shell_tool)
