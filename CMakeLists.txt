cmake_minimum_required(VERSION 3.8)
project(Onion)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "-g")
#set(CMAKE_C_FLAGS "-g -Wall -Werror")

set(SOURCE_FILES
        gethostbyname6.c
        gethostbyname6.h
        networks.c
        networks.h
        onion_router.c
        onion_router.h
        rsa.c
        rsa.h)

add_executable(Onion ${SOURCE_FILES})
target_link_libraries(Onion crypto)