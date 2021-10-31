cmake_minimum_required(VERSION 3.15)
project(Lab1Final C)

set(CMAKE_C_STANDARD 99)

add_executable(Lab1Final main.c disks_partitions_task/partition.h disks_partitions_task/partition.c hfs_task/utils.h hfs_task/utils.c hfs_task/hfs_structures.h hfs_task/hfs_structures.c)

add_library(hfsplus_utils SHARED hfs_task/utils.c hfs_task/hfs_structures.c disks_partitions_task/partition.c)
