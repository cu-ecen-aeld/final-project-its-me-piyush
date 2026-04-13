cmake_minimum_required(VERSION 3.10)
project(rover C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

include_directories(include)

set(CORE_SOURCES
    src/cbuf.c
    src/logger.c
    src/state_mgr.c
    src/motor_ctrl.c
    src/safety.c
    src/decision.c
)

# Production binary only -- no test targets in Yocto build
add_executable(rover ${CORE_SOURCES} src/main.c)
target_link_libraries(rover gpiod pthread m)
