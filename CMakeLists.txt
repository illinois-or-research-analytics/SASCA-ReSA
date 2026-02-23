cmake_minimum_required(VERSION 3.23)
#[[ In-source build check ]]
if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed.")
    message(WARNING "Remove CMakeFiles/ and CMakeCache.txt in the\
            source root before building again.")
else()
    #[[ Set project version ]]
    project(abm)
    # list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/external_libs/lib/cmake/igraph)
    # list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/external_libs/lib/cmake/libleidenalg)
    # list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/external_libs/lib64/cmake/igraph)
    # list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/external_libs/lib64/cmake/libleidenalg)
    # find_package(igraph 0.10 CONFIG REQUIRED)
    # find_package(libleidenalg 0.11 CONFIG REQUIRED)
    find_package(Eigen3 REQUIRED)
    find_package(OpenMP REQUIRED)

    set(ABM_MAJOR_VERSION 6)
    set(ABM_MINOR_VERSION 2)
    set(ABM_PATCH_VERSION 0)
    set(ABM_VERSION
        ${ABM_MAJOR_VERSION}.
        ${ABM_MINOR_VERSION}.
        ${ABM_PATCH_VERSION})


    #[[ CONSTANTS ]]
    # Compiler options
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clangd lsp
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native -funroll-loops -fomit-frame-pointer")
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer")
    # set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -ggdb -fno-omit-frame-pointer")
    # File paths
    set(SOURCE_MAIN ${CMAKE_SOURCE_DIR}/src/main.cpp)
    set(SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/abm.cpp
        ${CMAKE_SOURCE_DIR}/src/graph.cpp
        ${CMAKE_SOURCE_DIR}/src/utils.cpp
    )
    #[[ CONSTANTS END ]]

    #[[ Define the output binary ]]
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_executable(abm
        ${SOURCE_MAIN}
        ${SOURCE_FILES}
        )

    #[[ Link libraries ]]
    add_subdirectory(external_libs)
    target_link_libraries(abm PUBLIC OpenMP::OpenMP_CXX)
    target_link_libraries(abm PUBLIC Eigen3::Eigen)
    target_link_libraries(abm PUBLIC external_header_libs)

    target_include_directories(abm
        PUBLIC ${CMAKE_SOURCE_DIR}/includes
    )


endif("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
