cmake_minimum_required(VERSION 3.24)
project(pa4 C)

set(CMAKE_C_STANDARD 11)

add_library(rn SHARED IMPORTED)
set_target_properties(rn PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lib64/libruntime.so"
        )

include_directories(.)

add_executable(pa4
        common.h
        ipc.c
        ipc.h
        lamport.h lamport.c
        main.h
        work_manager.c
        work_manager.h
        main.c
        pa2345.h
        )

target_link_libraries(pa4 rn)
