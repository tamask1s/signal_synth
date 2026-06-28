cmake_minimum_required(VERSION 3.10)
project(signal_synth_test LANGUAGES CXX)

add_executable(
    signal_synth_test
    signal_synth_test.cpp
    ../src/signal_synth.cpp)

target_include_directories(signal_synth_test PRIVATE ../src)
target_compile_features(signal_synth_test PRIVATE cxx_std_11)

if(MSVC)
    target_compile_options(signal_synth_test PRIVATE /W4)
else()
    target_compile_options(signal_synth_test PRIVATE -Wall -Wextra -Wpedantic)
endif()
