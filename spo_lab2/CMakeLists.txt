cmake_minimum_required(VERSION 3.15)
project(Lab1Final C)

set(CMAKE_C_STANDARD 99)

add_library(hfsplus_utils SHARED hfs_task/utils.c hfs_task/hfs_structures.c disks_partitions_task/partition.c)

add_executable(lab1 main.c)

target_link_libraries(lab1 PRIVATE hfsplus_utils)
