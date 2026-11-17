cmake_minimum_required(VERSION 3.16)
project(polyglot VERSION 1.0.0 LANGUAGES CXX)

add_library(polyglot INTERFACE)
target_compile_features(polyglot INTERFACE cxx_std_23)
target_include_directories(polyglot INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(Catch2)

  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)

  add_executable(polyglot_tests tests/compile_check.cpp)
  target_link_libraries(polyglot_tests PRIVATE polyglot Catch2::Catch2WithMain)
  catch_discover_tests(polyglot_tests)

  function(add_polyglot_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE polyglot Catch2::Catch2WithMain)
    catch_discover_tests(${name})
  endfunction()

  add_polyglot_test(codec_binary_le_integer)
  add_polyglot_test(codec_binary_le_float)
  add_polyglot_test(codec_binary_be)
  add_polyglot_test(codec_ascii_integer)
  add_polyglot_test(codec_ascii_float)
  add_polyglot_test(codec_ascii_text)
  add_polyglot_test(schema_traits)
  add_polyglot_test(schema_config)
  add_polyglot_test(parser_markers)
  add_polyglot_test(parser_length)
  add_polyglot_test(parser_checksum)
  add_polyglot_test(parser_custom)
  add_polyglot_test(parser_partial)
  add_polyglot_test(parser_length_ranges)
endif()
