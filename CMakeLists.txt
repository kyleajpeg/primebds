cmake_minimum_required(VERSION 3.15)

project(primebds VERSION 3.4.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Endstone API version target
set(ENDSTONE_API_VERSION "0.11" CACHE STRING "Endstone API version")

include(FetchContent)

# Pin fmt to 10.2.1 BEFORE endstone fetches it (endstone defaults to fmt 11.2.0,
# which uses C23 glibc symbol redirects like __isoc23_strtol on glibc 2.38+ systems,
# causing GLIBC_2.38 runtime requirements that break older hosting environments)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

# Endstone framework
FetchContent_Declare(
    endstone
    GIT_REPOSITORY https://github.com/EndstoneMC/endstone.git
    GIT_TAG v${ENDSTONE_API_VERSION}
)
FetchContent_MakeAvailable(endstone)

# nlohmann/json for config management
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# SQLite3 (bundled amalgamation)
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Version header
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
configure_file(include/primebds/version.h.in include/primebds/version.h)

# Collect all source files
file(GLOB_RECURSE PLUGIN_SOURCES
    src/*.cpp
)

# Exclude Linux-specific glibc compatibility shim on non-Linux hosts.
# This file contains asm label declarations intended only for Linux/glibc
# build hosts and can trigger security scanners on other platforms.
if(NOT (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    list(REMOVE_ITEM PLUGIN_SOURCES ${CMAKE_SOURCE_DIR}/src/glibc_compat.cpp)
endif()

endstone_add_plugin(${PROJECT_NAME} ${PLUGIN_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    sqlite3
)

option(PRIMEBDS_BUILD_TESTS "Build permission, audit and player default regression tests" ON)
if(PRIMEBDS_BUILD_TESTS)
    enable_testing()
    add_executable(command_authorization_test tests/command_authorization_test.cpp)
    target_include_directories(command_authorization_test PRIVATE include)
    add_test(NAME command_authorization COMMAND command_authorization_test)
    add_executable(command_audit_test tests/command_audit_test.cpp)
    target_include_directories(command_audit_test PRIVATE include)
    add_test(NAME command_audit COMMAND command_audit_test)
    add_executable(user_message_defaults_test
        tests/user_message_defaults_test.cpp
        src/utils/database/user_db.cpp
        src/utils/database/database_manager.cpp)
    target_include_directories(user_message_defaults_test PRIVATE include)
    target_link_libraries(user_message_defaults_test PRIVATE nlohmann_json::nlohmann_json sqlite3)
    add_test(NAME user_message_defaults COMMAND user_message_defaults_test)
    add_executable(admin_commands_test tests/admin_commands_test.cpp)
    target_include_directories(admin_commands_test PRIVATE include)
    add_test(NAME admin_commands COMMAND admin_commands_test)
    add_executable(activity_summary_test tests/activity_summary_test.cpp
        src/utils/database/session_db.cpp src/utils/database/database_manager.cpp)
    target_include_directories(activity_summary_test PRIVATE include)
    target_link_libraries(activity_summary_test PRIVATE sqlite3)
    add_test(NAME activity_summary COMMAND activity_summary_test)
    add_test(NAME no_player_rtti COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/no_player_rtti.cmake)
endif()

# Copy plugin binary to output/ folder after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/output
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/output/
    COMMENT "Copying plugin to output/"
)
