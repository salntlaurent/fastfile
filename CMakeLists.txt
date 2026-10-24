cmake_minimum_required(VERSION 3.20)
project(FastFile VERSION 1.0 LANGUAGES CXX RC)
add_executable(FastFile WIN32
  src/main.cpp
  src/files.cpp
  src/platform.cpp
  src/settings.cpp
  src/settings_window.cpp
  src/shell.cpp
  resources/app.rc
)
target_compile_features(FastFile PRIVATE cxx_std_17)
target_compile_definitions(FastFile PRIVATE UNICODE _UNICODE NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_include_directories(FastFile PRIVATE resources)
target_link_libraries(FastFile PRIVATE comctl32 shell32 ole32 oleaut32 uuid advapi32 user32 gdi32)
if(MSVC)
  set_property(TARGET FastFile PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(FastFile PRIVATE /W4 /utf-8)
  target_link_options(FastFile PRIVATE /MANIFEST:NO)
else()
  target_compile_options(FastFile PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(FastFile PRIVATE -municode -static -static-libgcc -static-libstdc++ -s)
endif()
