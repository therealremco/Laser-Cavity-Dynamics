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

if(EMSCRIPTEN)
    # Browser version: emcmake cmake -S . -B build-web && cmake --build build-web
    # The finished site (HTML, JS and the .wasm module) ends up in build-web/site.
    # The core throws ParameterError for invalid input, so it needs exception support.
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
    target_compile_options(cavdyn PUBLIC -fwasm-exceptions)
    set(CAVDYN_SITE_DIR ${CMAKE_BINARY_DIR}/site)

    add_executable(cavdyn-web web/bindings.cpp)
    target_link_libraries(cavdyn-web PRIVATE cavdyn)
    target_compile_options(cavdyn-web PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(cavdyn-web PRIVATE
        -fwasm-exceptions
        -lembind
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createCavdyn
        -sENVIRONMENT=web,worker
        -sALLOW_MEMORY_GROWTH=1
    )
    set_target_properties(cavdyn-web PROPERTIES
        OUTPUT_NAME cavdyn
        SUFFIX .js
        RUNTIME_OUTPUT_DIRECTORY ${CAVDYN_SITE_DIR}
    )
    foreach(file index.html app.js plot.js worker.js style.css)
        configure_file(web/${file} ${CAVDYN_SITE_DIR}/${file} COPYONLY)
    endforeach()
    configure_file(LICENSE ${CAVDYN_SITE_DIR}/LICENSE.txt COPYONLY)
    return()
endif()

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
