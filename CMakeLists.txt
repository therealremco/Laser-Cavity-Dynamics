cmake_minimum_required(VERSION 3.16)
project(cavity-dynamics VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

add_library(cavdyn
    src/Cavity.cpp
    src/GainMedium.cpp
    src/Grid.cpp
    src/Parameters.cpp
    src/Pump.cpp
    src/Simulator.cpp
)
target_include_directories(cavdyn PUBLIC include)
target_compile_features(cavdyn PUBLIC cxx_std_17)
target_compile_options(cavdyn PRIVATE -Wall -Wextra -Wpedantic)

add_library(cavdyn_io
    io/ConfigReader.cpp
    io/ResultWriter.cpp
)
target_include_directories(cavdyn_io PUBLIC include io)
target_link_libraries(cavdyn_io PUBLIC cavdyn)
target_compile_features(cavdyn_io PUBLIC cxx_std_17)
target_compile_options(cavdyn_io PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cavity-dynamics apps/cli/main.cpp)
target_link_libraries(cavity-dynamics PRIVATE cavdyn_io)

if(BUILD_TESTING)
    add_executable(cavdyn-tests tests/test_simulator.cpp)
    target_link_libraries(cavdyn-tests PRIVATE cavdyn_io)
    add_test(NAME cavdyn-tests COMMAND cavdyn-tests)
endif()
