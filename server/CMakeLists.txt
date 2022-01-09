cmake_minimum_required(VERSION 3.21)
project(server)
set(CMAKE_CXX_STANDARD 20)
find_package(Boost 1.76.0 REQUIRED COMPONENTS random system filesystem )

if (Boost_FOUND)
    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(server main.cpp constants.h server.h)
    target_link_libraries(server ${Boost_LIBRARIES})
endif ()
