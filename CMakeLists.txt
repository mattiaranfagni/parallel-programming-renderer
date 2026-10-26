cmake_minimum_required(VERSION 3.20)
project(Renderer CXX)

# Forza l'utilizzo dello standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# AIUTO PER CMAKE SU MAC M1: Dice a CMake dove cercare libomp di Homebrew
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()

# Ora find_package troverà OpenMP senza fallire
find_package(OpenMP REQUIRED)

add_executable(Renderer main.cpp)

# Collega le librerie di OpenMP al tuo target
if(OpenMP_CXX_FOUND)
    target_link_libraries(Renderer PRIVATE OpenMP::OpenMP_CXX)
endif()
