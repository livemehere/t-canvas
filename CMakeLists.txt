cmake_minimum_required(VERSION 4.2)

project(TCanvas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(app src/main.cpp)