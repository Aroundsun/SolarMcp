# 为插件添加共享库
set(PLUGIN_LIB_DIR ${CMAKE_SOURCE_DIR}/plugins/lib)

# 最小示例插件（纯 C ABI）
add_library(minimal_plugin MODULE
    example/minimal_plugin.c
)
target_include_directories(minimal_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(minimal_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/lib
    C_STANDARD 11
)

# Shell Tool 插件（C ABI 注册，逻辑在同 .so 内）
add_library(shell_plugin MODULE
    shell/shell_tool.cpp
    shell/shell_plugin.cpp
)
target_include_directories(shell_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/plugins
)
target_link_libraries(shell_plugin PRIVATE
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
)
set_target_properties(shell_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/lib
)

foreach(_plugin minimal_plugin shell_plugin)
    add_custom_command(TARGET ${_plugin} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_LIB_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${_plugin}>
            ${PLUGIN_LIB_DIR}/
        COMMENT "Copying ${_plugin} to plugins/lib/"
    )
endforeach()
