cmake_minimum_required(VERSION 3.17)
project(SPO_lab3 C)
set(CMAKE_C_STANDARD 99)

add_executable(lab3 main.c server.c client.c common.c)
add_executable(gui gui.c common.c)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(lab3 PRIVATE Threads::Threads)
target_link_libraries(gui PRIVATE ncurses Threads::Threads)