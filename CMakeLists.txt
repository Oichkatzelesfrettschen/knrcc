cmake_minimum_required(VERSION 3.10)
project(knrcc_modernized C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add an include directory for headers located in c/
include_directories(c)

# Define the source files for the compiler passes
set(PASS_SOURCES
    c/c00.c
    c/c01.c
    c/c02.c
    c/c03.c
    c/c04.c
    c/c05.c
    c/c10.c
    c/c11.c
    c/c12.c
    c/c13.c
    c/c20.c
    c/c21.c
)

# Create the main compiler executable (knrcc)
add_executable(knrcc cc.c ${PASS_SOURCES})

# Create the cvopt executable
add_executable(cvopt c/cvopt.c)

# Optional: Add a target for c0, c1, c2 if they are meant to be separate
# For now, we assume cc.c is the main driver for all passes.
# If c0, c1, c2 were separate executables driven by makefile:
# add_executable(c0_exe ${C0_SOURCES})
# add_executable(c1_exe ${C1_SOURCES})
# add_executable(c2_exe ${C2_SOURCES})
# We might need to link them or ensure cc.c can call their main functions.

# Add build options to treat warnings as errors, and enable common warnings.
# This will be useful for the refactoring process.
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror")
endif()
