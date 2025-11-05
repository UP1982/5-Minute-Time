cmake_minimum_required(VERSION 3.20)
project(SimpleTimer LANGUAGES CXX)

add_executable(SimpleTimer WIN32 src/main.cpp)

target_compile_features(SimpleTimer PRIVATE cxx_std_20)

target_link_libraries(SimpleTimer PRIVATE winmm)
