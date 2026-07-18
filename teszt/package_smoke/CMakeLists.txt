cmake_minimum_required(VERSION 3.10)
project(signal_synth_package_smoke LANGUAGES CXX)

find_package(signal_synth 0.9.0 EXACT CONFIG REQUIRED)

add_executable(signal_synth_package_smoke main.cpp)
target_compile_features(signal_synth_package_smoke PRIVATE cxx_std_11)
set_target_properties(signal_synth_package_smoke PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(signal_synth_package_smoke PRIVATE signal_synth::signal_synth)
