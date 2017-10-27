cmake_minimum_required( VERSION 3.0 )

set(CMAKE_CXX_STANDARD 14)

file(GLOB sources "*.cpp")
file(GLOB headers "*.h")

add_executable(porosity ${sources} ${headers})
