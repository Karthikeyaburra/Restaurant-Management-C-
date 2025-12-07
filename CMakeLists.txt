cmake_minimum_required(VERSION 3.16)
project(To_Do)

set(CMAKE_CXX_STANDARD 20)

add_executable(To_Do
        main.cpp
)

# ✅ Project root (crow.h, crow/)
target_include_directories(To_Do PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# ✅ Exact boost folder
target_include_directories(To_Do PRIVATE
        ${CMAKE_SOURCE_DIR}/boost
)

# ✅ REQUIRED Windows networking libraries
if (WIN32)
    target_link_libraries(To_Do ws2_32 mswsock)
endif ()
