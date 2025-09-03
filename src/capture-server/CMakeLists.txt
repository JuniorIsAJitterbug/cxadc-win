cmake_minimum_required(VERSION 3.14)
project(cxadc_vhs_server C)

set(CMAKE_C_STANDARD 11)

add_executable(cxadc_vhs_server
        main.c
        http.c
        files.c)

target_compile_options(cxadc_vhs_server PRIVATE
        -Wall
        -Wpedantic
        -Wextra)

target_compile_definitions(cxadc_vhs_server PRIVATE
        CXADC_VHS_SERVER_MAJOR=1
        CXADC_VHS_SERVER_MINOR=4
        CXADC_VHS_SERVER_PATCH=0
)

target_link_libraries(cxadc_vhs_server PRIVATE
        asound
        pthread)
