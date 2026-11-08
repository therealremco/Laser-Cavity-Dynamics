cmake_minimum_required(VERSION 3.16)
project(cavity-dynamics VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

add_library(cavdyn
    src/Cavity.cpp
    src/GainMedium.cpp
    src/Grid.cpp
    src/Parameters.cpp
    src/Pump.cpp
    src/Results.cpp
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
target_compile_definitions(cavdyn_io PRIVATE CAVDYN_VERSION="${PROJECT_VERSION}")

add_executable(cavity-dynamics apps/cli/main.cpp)
target_link_libraries(cavity-dynamics PRIVATE cavdyn_io)

if(BUILD_TESTING)
    # One executable per area. Each takes an optional test name as argument.
    foreach(area io components simulator validation)
        add_executable(cavdyn-${area} tests/test_${area}.cpp)
        target_link_libraries(cavdyn-${area} PRIVATE cavdyn_io)
        target_compile_options(cavdyn-${area} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
    target_compile_definitions(cavdyn-io PRIVATE CAVDYN_TEST_VERSION="${PROJECT_VERSION}")

    add_test(NAME io COMMAND cavdyn-io)
    add_test(NAME components COMMAND cavdyn-components)
    add_test(NAME simulator COMMAND cavdyn-simulator)
    # The validation cases from CONTEXT.md, each as its own test.
    foreach(case lumped_limit_threshold lumped_limit_dynamics relaxation_oscillations
                 conservation propagation convergence legacy_comparison)
        add_test(NAME validation.${case} COMMAND cavdyn-validation ${case})
    endforeach()
endif()
