# ---------------------------------------------------------------------------
# 第三方依赖：优先系统包，回退 FetchContent
# 从项目根目录包含，使导入目标在各处可见。
# ---------------------------------------------------------------------------
include(FetchContent)

# nlohmann/json — JSON 解析（仅头文件）
find_package(nlohmann_json 3.11.3 QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "Using system nlohmann_json")
else()
    message(STATUS "nlohmann_json not found; fetching via FetchContent")
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# yaml-cpp — YAML 配置解析
find_package(yaml-cpp 0.8 QUIET)
if(yaml-cpp_FOUND)
    message(STATUS "Using system yaml-cpp")
else()
    message(STATUS "yaml-cpp not found; fetching via FetchContent")
    FetchContent_Declare(yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

# fmtlib — 现代 C++ 字符串格式化
find_package(fmt 10 QUIET)
if(fmt_FOUND)
    message(STATUS "Using system fmt")
else()
    message(STATUS "fmt not found; fetching via FetchContent")
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        10.2.1
    )
    FetchContent_MakeAvailable(fmt)
endif()

# GoogleTest — 单元测试框架
find_package(GTest 1.14 QUIET)
if(GTest_FOUND)
    message(STATUS "Using system GTest")
else()
    message(STATUS "GTest not found; fetching via FetchContent")
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()
