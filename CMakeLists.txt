cmake_minimum_required(VERSION 3.20)
project(trading_microservices CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# -fno-omit-frame-pointer so perf call graphs are readable
add_compile_options(-Wall -Wextra -Wno-format-security -fno-omit-frame-pointer)

option(SANITIZE "Build with ASan+UBSan" OFF)
if(SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()

# Boost.Asio is header-only (Boost >= 1.69 has a header-only Boost.System).
find_path(BOOST_INCLUDE boost/asio.hpp REQUIRED)
find_package(Threads REQUIRED)

foreach(t md_svc risk_svc gw_svc)
  string(REPLACE "_svc" "" dir ${t})
  add_executable(${t} src/${dir}/${t}.cpp)
  target_include_directories(${t} SYSTEM PRIVATE ${BOOST_INCLUDE})
  target_link_libraries(${t} PRIVATE Threads::Threads)
endforeach()

add_executable(loadgen src/loadgen/loadgen.cpp)
target_link_libraries(loadgen PRIVATE Threads::Threads)

add_executable(unit_tests tests/unit_tests.cpp)
target_include_directories(unit_tests SYSTEM PRIVATE ${BOOST_INCLUDE})
target_link_libraries(unit_tests PRIVATE Threads::Threads)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
