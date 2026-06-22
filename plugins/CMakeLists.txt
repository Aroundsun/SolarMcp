# 为插件添加共享库
add_library(read_file_plugin MODULE
    filesystem/read_file_plugin.cpp
)
target_include_directories(read_file_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(read_file_plugin PRIVATE
    nlohmann_json::nlohmann_json
)
# 确保使用 .so 扩展名（而非 .dylib 或 .dll）
set_target_properties(read_file_plugin PROPERTIES
    PREFIX ""
    SUFFIX ".so"
)
