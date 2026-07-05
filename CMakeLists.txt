cmake_minimum_required(VERSION 4.2)

project(TCanvas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE SDL3::SDL3)