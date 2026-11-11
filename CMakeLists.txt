cmake_minimum_required(VERSION 3.20)
project(HeroNativeEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)
add_executable(hero-native-engine src/main.cpp)
target_include_directories(hero-native-engine PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_options(hero-native-engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

install(TARGETS hero-native-engine RUNTIME DESTINATION bin)
