# SolarMcp 单元测试

# 收集所有测试源文件
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
