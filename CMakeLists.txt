cmake_minimum_required(VERSION 3.20)
project(FiveMinuteTimer LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "The 5-minute timer application must be built on Windows.")
endif()

add_executable(FiveMinuteTimer
    src/main.cpp
    src/CountdownWindow.cpp
    src/CountdownWindow.h
    src/ConfettiWindow.cpp
    src/ConfettiWindow.h
)

set_target_properties(FiveMinuteTimer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    WIN32_EXECUTABLE YES
)

target_include_directories(FiveMinuteTimer PRIVATE src)

target_compile_definitions(FiveMinuteTimer PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00)

target_link_libraries(FiveMinuteTimer PRIVATE winmm Shcore)
