cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_model STATIC
    src/model.cpp
    src/model.h
    src/tagged.h
    src/geom.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/collision_detector.cpp
    src/collision_detector.h
    src/extra_data.cpp
    src/extra_data.h
)
target_include_directories(game_model PUBLIC src)
target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.h
    src/logging_request_handler.h
    src/logging.h
    src/logging.cpp
)
target_link_libraries(game_server PRIVATE game_model CONAN_PKG::boost Threads::Threads)

add_executable(game_server_tests
    tests/loot_generator_tests.cpp
)
target_link_libraries(game_server_tests PRIVATE game_model CONAN_PKG::catch2)

include(CTest)
if(DEFINED CONAN_BUILD_DIRS_CATCH2_RELEASE AND EXISTS "${CONAN_BUILD_DIRS_CATCH2_RELEASE}/Catch.cmake")
    include(${CONAN_BUILD_DIRS_CATCH2_RELEASE}/Catch.cmake)
    catch_discover_tests(game_server_tests)
elseif(DEFINED CONAN_BUILD_DIRS_CATCH2_DEBUG AND EXISTS "${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake")
    include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
    catch_discover_tests(game_server_tests)
endif()
